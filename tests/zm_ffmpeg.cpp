/*
 * This file is part of the ZoneMinder Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "zm_catch2.h"

#include "zm_ffmpeg.h"

#include <string>

TEST_CASE("get_decoder_data returns only real decoders for the wanted codec", "[ffmpeg]") {
  for (AVCodecID codec_id : {AV_CODEC_ID_H264, AV_CODEC_ID_H265}) {
    std::list<const CodecData *> results = get_decoder_data(codec_id, "auto");

    // The plain software decoder is always compiled into ffmpeg
    REQUIRE(!results.empty());

    for (const CodecData *entry : results) {
      CHECK(entry->codec_id == codec_id);

      const AVCodec *codec = avcodec_find_decoder_by_name(entry->codec_name);
      INFO("table entry " << entry->codec_name << " must exist as a decoder");
      REQUIRE(codec != nullptr);
      CHECK(av_codec_is_decoder(codec) != 0);
    }
  }
}

TEST_CASE("get_decoder_data honours an explicit decoder name", "[ffmpeg]") {
  std::list<const CodecData *> results = get_decoder_data(AV_CODEC_ID_H265, "hevc");

  REQUIRE(results.size() == 1);
  CHECK(std::string(results.front()->codec_name) == "hevc");

  // A name belonging to another codec id yields nothing
  results = get_decoder_data(AV_CODEC_ID_H264, "hevc");
  CHECK(results.empty());
}

TEST_CASE("get_decoder_data prefers decoders matching the requested hwaccel", "[ffmpeg]") {
#if HAVE_LIBAVUTIL_HWCONTEXT_H && LIBAVCODEC_VERSION_CHECK(57, 107, 0, 107, 0)
  std::list<const CodecData *> results = get_decoder_data(AV_CODEC_ID_H265, "auto", "qsv");

  REQUIRE(!results.empty());

  // All candidates matching the requested type must come before any others,
  // so that a usable hw wrapper decoder (e.g. hevc_qsv) is tried before the
  // plain software decoder opens successfully and wins.
  bool seen_non_matching = false;
  for (const CodecData *entry : results) {
    if (entry->hwdevice_type == AV_HWDEVICE_TYPE_QSV) {
      INFO(entry->codec_name << " matches qsv but is listed after a non-qsv decoder");
      CHECK(!seen_non_matching);
    } else {
      seen_non_matching = true;
    }
  }

  // If this ffmpeg build ships the qsv wrapper, it must be the first candidate
  if (avcodec_find_decoder_by_name("hevc_qsv")) {
    CHECK(std::string(results.front()->codec_name) == "hevc_qsv");
  }

  // An unknown or empty hwaccel name behaves as before: plain decoder order
  results = get_decoder_data(AV_CODEC_ID_H265, "auto", "");
  REQUIRE(!results.empty());
  CHECK(std::string(results.front()->codec_name) == "hevc");
#endif
}
