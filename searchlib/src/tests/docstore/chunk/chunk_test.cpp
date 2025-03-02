// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/searchlib/docstore/chunk.h>
#include <vespa/searchlib/docstore/chunkformat.h>
#include <vespa/searchlib/docstore/chunkformats.h>
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/objects/hexdump.h>
#include <string>
#include <zstd.h>

#include <vespa/log/log.h>
LOG_SETUP("chunk_test");

using namespace search;
using vespalib::compression::CompressionConfig;

TEST(ChunkTest, require_that_Chunk_obey_limits)
{
    Chunk c(0, Chunk::Config(256));
    EXPECT_TRUE(c.hasRoom(1000)); // At least 1 is allowed no matter what the size is.
    c.append(1, {"abc", 3});
    EXPECT_TRUE(c.hasRoom(229));
    EXPECT_FALSE(c.hasRoom(230));
    c.append(2, {"abc", 3});
    EXPECT_TRUE(c.hasRoom(20));
}

TEST(ChunkTest, require_that_Chunk_can_produce_unique_list)
{
    const char *d = "ABCDEF";
    Chunk c(0, Chunk::Config(100));
    c.append(1, {d, 1});
    c.append(2, {d, 2});
    c.append(3, {d, 3});
    c.append(2, {d, 4});
    c.append(1, {d, 5});
    EXPECT_EQ(5u, c.count());
    const Chunk::LidList & all = c.getLids();
    EXPECT_EQ(5u, all.size());
    Chunk::LidList unique = c.getUniqueLids();
    EXPECT_EQ(3u, unique.size());
    EXPECT_EQ(1u, unique[0].getLid());
    EXPECT_EQ(5u, unique[0].netSize());
    EXPECT_EQ(2u, unique[1].getLid());
    EXPECT_EQ(4u, unique[1].netSize());
    EXPECT_EQ(3u, unique[2].getLid());
    EXPECT_EQ(3u, unique[2].netSize());
}

void testChunkFormat(ChunkFormat &cf, size_t expectedLen, const std::string &expectedContent)
{
    CompressionConfig cfg;
    uint64_t MAGIC_CONTENT(0xabcdef9876543210);
    cf.getBuffer() << MAGIC_CONTENT;
    vespalib::DataBuffer buffer;
    cf.pack(7, buffer, cfg);
    EXPECT_EQ(expectedLen, buffer.getDataLen());
    std::ostringstream os;
    os << vespalib::HexDump(buffer.getData(), buffer.getDataLen());
    EXPECT_EQ(expectedContent, os.str());
}

TEST(ChunkTest, require_that_Chunk_formats_does_not_change_between_releases)
{
    ChunkFormatV1 v1(10);
    testChunkFormat(v1, 26, "26 000000000010ABCDEF987654321000000000000000079CF5E79B");
    ChunkFormatV2 v2(10);
    testChunkFormat(v2, 34, "34 015BA32DE7000000220000000010ABCDEF987654321000000000000000074D000694");
}

constexpr const char * MY_LONG_STRING = "This is medium long string that hopefully will compress to something where lz4, zstandard and none"
" will make a difference. The intentions is to verify that we trigger all compresssions possible and are able to decompress them too."
" I guess that we need a considerable length in order to get the rather inefficient lz4 compression triger. ZStandard compression"
" should trigger a lot earlier";

void verifyChunkCompression(CompressionConfig::Type cfgType, const void * buf, size_t sz, size_t expectedLen) {
    uint64_t MAGIC_CONTENT(0xabcdef9876543210);
    ChunkFormatV2 chunk(10);
    chunk.getBuffer() << MAGIC_CONTENT;
    chunk.getBuffer().write(buf, sz);
    vespalib::DataBuffer buffer;
    CompressionConfig cfg(cfgType);
    chunk.pack(7, buffer, cfg);
    EXPECT_EQ(expectedLen, buffer.getDataLen());
    vespalib::nbostream is(buffer.getData(), buffer.getDataLen());
    ChunkFormat::UP deserialized = ChunkFormat::deserialize(buffer.getData(), buffer.getDataLen());
    uint64_t magic(0);
    deserialized->getBuffer() >> magic;
    EXPECT_EQ(MAGIC_CONTENT, magic);
    std::vector<char> v(sz);
    deserialized->getBuffer().read(&v[0], sz);
    EXPECT_EQ(0, memcmp(buf, &v[0], sz));
}

TEST(ChunkTest, require_that_V2_can_create_and_handle_lz4_zstd_and_none) {
    verifyChunkCompression(CompressionConfig::NONE, MY_LONG_STRING, strlen(MY_LONG_STRING), 421);
    verifyChunkCompression(CompressionConfig::LZ4, MY_LONG_STRING, strlen(MY_LONG_STRING), 360);
    constexpr size_t zstd_compressed_length = (ZSTD_VERSION_NUMBER >= 10407) ? 284 : 282;
    verifyChunkCompression(CompressionConfig::ZSTD, MY_LONG_STRING, strlen(MY_LONG_STRING), zstd_compressed_length);
}

GTEST_MAIN_RUN_ALL_TESTS()
