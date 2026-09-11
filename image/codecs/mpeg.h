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

#ifndef IMAGE_CODECS_MPEG_H
#define IMAGE_CODECS_MPEG_H

#include "common/array.h"
#include "image/codecs/codec.h"
#include "graphics/pixelformat.h"

typedef struct mpeg2dec_s mpeg2dec_t;
typedef struct mpeg2_info_s mpeg2_info_t;

namespace Common {
class SeekableReadStream;
}

namespace Graphics {
struct Surface;
}

namespace Image {

/** An owned, tightly packed YUV420 display picture and its original timing. */
struct MPEGFrame {
	MPEGFrame() : width(0), height(0), progressive(false), topFieldFirst(false),
			fieldCount(0), period(0), pts(0xFFFFFFFF) {}

	void create(uint16 w, uint16 h) {
		width = w;
		height = h;
		planes[0].resize((uint32)w * h);
		planes[1].resize((uint32)(w / 2) * (h / 2));
		planes[2].resize((uint32)(w / 2) * (h / 2));
	}

	Common::Array<byte> planes[3];
	uint16 width, height;
	bool progressive, topFieldFirst;
	uint fieldCount;
	uint32 period; // Display duration in 27 MHz ticks.
	uint32 pts;    // Original packet timestamp in 90 kHz ticks, or unknown.
};

/**
 * MPEG 1/2 video decoder.
 *
 * Used by BMP/AVI.
 */
class MPEGDecoder : public Codec {
public:
	MPEGDecoder();
	~MPEGDecoder() override;

	// Codec interface
	const Graphics::Surface *decodeFrame(Common::SeekableReadStream &stream) override;
	Graphics::PixelFormat getPixelFormat() const override { return _pixelFormat; }
	bool setOutputPixelFormat(const Graphics::PixelFormat &format) override {
		if (format.bytesPerPixel != 2 && format.bytesPerPixel != 4)
			return false;
		_pixelFormat = format;
		return true;
	}

	// MPEGPSDecoder call
	bool decodePacket(Common::SeekableReadStream &packet, uint32 &framePeriod, Graphics::Surface *dst = 0);

	// Resumable raw output. Do not mix this interface with decodePacket().
	enum PictureResult { kPictureReady, kNeedsInput, kDrained, kUnsupported };
	// Takes ownership; call only after decodePicture() returns kNeedsInput.
	void queuePacket(Common::SeekableReadStream *packet, uint32 pts);
	// Signal demuxer EOF. Subsequent decodePicture() calls drain the codec.
	void finish();
	PictureResult decodePicture(MPEGFrame &frame);
	void convertFrame(const MPEGFrame &frame, Graphics::Surface *dst);

private:
	Graphics::PixelFormat _pixelFormat;
	Graphics::Surface *_surface;

	enum {
		BUFFER_SIZE = 4096
	};

	byte _buffer[BUFFER_SIZE];
	mpeg2dec_t *_mpegDecoder;
	const mpeg2_info_t *_mpegInfo;

	Common::SeekableReadStream *_packet;
	uint32 _packetPts;
	bool _packetStart;
	bool _finishing, _drainSent, _sequenceEnded, _rawFailed, _rawHasPicture;
	uint16 _rawWidth, _rawHeight;
	bool copyPicture(MPEGFrame &frame);
};

} // End of namespace Image

#endif // IMAGE_CODECS_MPEG_H
