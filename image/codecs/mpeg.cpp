/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/debug.h"
#include "common/scummsys.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/surface.h"
#include "graphics/yuv_to_rgb.h"

#include "image/codecs/mpeg.h"

extern "C" {
	#include <mpeg2dec/mpeg2.h>
}

namespace Image {

MPEGDecoder::MPEGDecoder() : Codec(), _packet(0), _packetPts(0xFFFFFFFF),
		_packetStart(false), _finishing(false), _drainSent(false),
		_sequenceEnded(false), _rawFailed(false), _rawHasPicture(false), _rawWidth(0), _rawHeight(0) {
	_pixelFormat = getDefaultYUVFormat();

	_surface = 0;

	_mpegDecoder = mpeg2_init();

	if (!_mpegDecoder)
		error("Could not initialize libmpeg2");

	_mpegInfo = mpeg2_info(_mpegDecoder);
}

MPEGDecoder::~MPEGDecoder() {
	delete _packet;
	mpeg2_close(_mpegDecoder);

	if (_surface) {
		_surface->free();
		delete _surface;
	}
}

const Graphics::Surface *MPEGDecoder::decodeFrame(Common::SeekableReadStream &stream) {
	uint32 framePeriod;
	decodePacket(stream, framePeriod);
	return _surface;
}

bool MPEGDecoder::decodePacket(Common::SeekableReadStream &packet, uint32 &framePeriod, Graphics::Surface *dst) {
	// Decode as much as we can out of this packet
	uint32 size = 0xFFFFFFFF;
	mpeg2_state_t state;
	bool foundFrame = false;
	framePeriod = 0;

	do {
		state = mpeg2_parse(_mpegDecoder);

		switch (state) {
		case STATE_BUFFER:
			size = packet.read(_buffer, BUFFER_SIZE);
			mpeg2_buffer(_mpegDecoder, _buffer, _buffer + size);
			break;
		case STATE_SLICE:
		case STATE_END:
			if (_mpegInfo->display_fbuf) {
				foundFrame = true;
				const mpeg2_sequence_t *sequence = _mpegInfo->sequence;
				const mpeg2_picture_t *picture = _mpegInfo->display_picture;

				framePeriod += sequence->frame_period;
				if (picture->nb_fields > 2) {
					framePeriod += (sequence->frame_period / 2);

				}

				if (!dst) {
					// If no destination is specified, use our internal storage
					if (!_surface) {
						_surface = new Graphics::Surface();
						_surface->create(sequence->picture_width, sequence->picture_height, _pixelFormat);
					}

					dst = _surface;
				}

				YUVToRGBMan.convert420(dst, Graphics::YUVToRGBManager::kScaleITU, _mpegInfo->display_fbuf->buf[0],
						_mpegInfo->display_fbuf->buf[1], _mpegInfo->display_fbuf->buf[2], sequence->picture_width,
						sequence->picture_height, sequence->width, sequence->chroma_width);
			}
			break;
		default:
			break;
		}
	} while (size != 0);

	return foundFrame;
}

void MPEGDecoder::queuePacket(Common::SeekableReadStream *packet, uint32 pts) {
	assert(!_packet && !_finishing);
	_packet = packet;
	_packetPts = pts;
	_packetStart = true;
}

void MPEGDecoder::finish() {
	_finishing = true;
}

MPEGDecoder::PictureResult MPEGDecoder::decodePicture(MPEGFrame &frame) {
	if (_rawFailed)
		return kUnsupported;

	for (;;) {
		const mpeg2_state_t state = mpeg2_parse(_mpegDecoder);
		switch (state) {
		case STATE_BUFFER: {
			uint32 size = _packet ? _packet->read(_buffer, BUFFER_SIZE) : 0;
			if (size) {
				// Tag the byte position where this PES payload starts. libmpeg2
				// carries it through picture reordering to display_picture.
				if (_packetStart && _packetPts != 0xFFFFFFFF)
					mpeg2_tag_picture(_mpegDecoder, _packetPts, 0);
				_packetStart = false;
			} else {
				delete _packet;
				_packet = 0;
				if (!_finishing)
					return kNeedsInput;
				if (_drainSent || _sequenceEnded)
					return kDrained;
				// A sequence end releases the last reference picture, even
				// when the file ended without an MPEG sequence-end code.
				_buffer[0] = _buffer[1] = 0;
				_buffer[2] = 1;
				_buffer[3] = 0xB7;
				size = 4;
				_drainSent = true;
			}
			mpeg2_buffer(_mpegDecoder, _buffer, _buffer + size);
			break;
		}
		case STATE_SEQUENCE:
		case STATE_SEQUENCE_REPEATED:
		case STATE_SEQUENCE_MODIFIED: {
			if (state == STATE_SEQUENCE)
				_rawHasPicture = false;
			const mpeg2_sequence_t &s = *_mpegInfo->sequence;
			if (!s.picture_width || !s.picture_height ||
					(s.picture_width & 1) || (s.picture_height & 1) ||
					s.picture_width > 0xFFFF || s.picture_height > 0xFFFF ||
					s.picture_width > s.width || s.picture_height > s.height ||
					s.chroma_width * 2 != s.width || s.chroma_height * 2 != s.height ||
					(_rawWidth && (_rawWidth != s.picture_width || _rawHeight != s.picture_height))) {
				_rawFailed = true;
				return kUnsupported;
			}
			_rawWidth = s.picture_width;
			_rawHeight = s.picture_height;
			_sequenceEnded = false;
			break;
		}
		case STATE_PICTURE:
			_sequenceEnded = false;
			break;
		case STATE_SLICE:
		case STATE_END:
		case STATE_INVALID_END:
			if (state == STATE_SLICE)
				_rawHasPicture = true;
			// A new sequence with different storage padding also releases the
			// old reference picture, before its new layout is reported.
			_sequenceEnded = state != STATE_SLICE;
			// A sequence header without any slices can leave stale metadata
			// in libmpeg2's end-of-sequence display slot.
			if (_rawHasPicture && _mpegInfo->display_fbuf && _mpegInfo->display_picture && copyPicture(frame))
				return kPictureReady;
			break;
		default:
			break;
		}
	}
}

bool MPEGDecoder::copyPicture(MPEGFrame &frame) {
	const mpeg2_sequence_t &s = *_mpegInfo->sequence;
	const mpeg2_picture_t &p = *_mpegInfo->display_picture;
	// Do not expose an incomplete field pair as a whole display picture.
	uint fields = p.nb_fields;
	if (_mpegInfo->display_picture_2nd)
		fields += _mpegInfo->display_picture_2nd->nb_fields;
	if (fields < 2 || !_rawWidth)
		return false;

	frame.create(_rawWidth, _rawHeight);
	for (uint plane = 0; plane < 3; ++plane) {
		const uint width = plane ? frame.width / 2 : frame.width;
		const uint height = plane ? frame.height / 2 : frame.height;
		const uint pitch = plane ? s.chroma_width : s.width;
		const byte *src = _mpegInfo->display_fbuf->buf[plane];
		for (uint y = 0; y < height; ++y)
			memcpy(&frame.planes[plane][y * width], src + y * pitch, width);
	}
	frame.progressive = (s.flags & SEQ_FLAG_PROGRESSIVE_SEQUENCE) || (p.flags & PIC_FLAG_PROGRESSIVE_FRAME);
	frame.topFieldFirst = (p.flags & PIC_FLAG_TOP_FIELD_FIRST) != 0;
	frame.fieldCount = fields;
	frame.period = (uint64)s.frame_period * fields / 2;
	frame.pts = (p.flags & PIC_FLAG_TAGS) ? p.tag : 0xFFFFFFFF;
	return true;
}

void MPEGDecoder::convertFrame(const MPEGFrame &frame, Graphics::Surface *dst) {
	assert(dst && dst->w == frame.width && dst->h == frame.height);
	YUVToRGBMan.convert420(dst, Graphics::YUVToRGBManager::kScaleITU,
			&frame.planes[0][0], &frame.planes[1][0], &frame.planes[2][0],
			frame.width, frame.height, frame.width, frame.width / 2);
}

} // End of namespace Image
