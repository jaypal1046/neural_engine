// =============================================================================
// Block-Based Random Access Implementation
// Allows decompressing specific blocks without reading the entire file
// Perfect for knowledge modules where only small portions are needed
// =============================================================================

#include "compressor.h"
#include "ans.h"
#include "bwt.h"
#include "ppm.h"
#include "cmix.h"
#include <cstdio>
#include <cstring>
#include <vector>

// Internal handle structure (opaque to caller)
struct BlockFileHandle {
    FILE*                    fp;
    BlockFileInfo            info;
    std::vector<BlockInfo>   blocks;
    uint64_t                 data_start;  // Offset where block data begins
};

// Helper: read uint32_t little-endian
static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Open compressed file for random access
void* block_open(const std::string& file_path, BlockFileInfo& info) {
    FILE* fp = fopen(file_path.c_str(), "rb");
    if (!fp) return nullptr;

    // Read 54-byte header
    uint8_t hdr[54];
    if (fread(hdr, 1, 54, fp) != 54) {
        fclose(fp);
        return nullptr;
    }

    // Validate magic
    if (hdr[0] != 'M' || hdr[1] != 'Z' || hdr[2] != 'I' || hdr[3] != 'P') {
        fclose(fp);
        return nullptr;
    }

    // Parse header
    uint8_t version = hdr[4];
    if (version < 5 || version > 10) {
        fclose(fp);
        return nullptr;
    }

    uint8_t mode = hdr[5];
    uint64_t orig_size = 0;
    for (int i = 0; i < 8; ++i) orig_size |= (uint64_t)hdr[6+i] << (i*8);

    uint32_t block_count = read_u32le(hdr + 46);
    uint32_t block_size = read_u32le(hdr + 50);

    // Fill info
    info.version = version;
    info.mode = mode;
    info.orig_size = orig_size;
    info.block_count = block_count;
    info.block_size = block_size;
    memcpy(info.sha256, hdr + 14, 32);

    // For STORED mode, no block index
    if (mode == 0x01) {
        auto* handle = new BlockFileHandle;
        handle->fp = fp;
        handle->info = info;
        handle->data_start = 54;
        return handle;
    }

    // Read block index (block_count * 8 bytes)
    std::vector<BlockInfo> blocks(block_count);
    uint64_t offset = 54 + (uint64_t)block_count * 8;  // Data starts after index

    for (uint32_t b = 0; b < block_count; ++b) {
        uint8_t entry[8];
        if (fread(entry, 1, 8, fp) != 8) {
            fclose(fp);
            return nullptr;
        }

        blocks[b].compressed_size = read_u32le(entry + 0);
        blocks[b].original_size = read_u32le(entry + 4);
        blocks[b].file_offset = offset;
        offset += blocks[b].compressed_size;
    }

    // Create handle
    auto* handle = new BlockFileHandle;
    handle->fp = fp;
    handle->info = info;
    handle->blocks = std::move(blocks);
    handle->data_start = 54 + (uint64_t)block_count * 8;

    return handle;
}

// Get information about a specific block
bool block_get_info(void* handle, uint32_t block_index, BlockInfo& info) {
    if (!handle) return false;
    auto* h = static_cast<BlockFileHandle*>(handle);

    if (block_index >= h->info.block_count) return false;

    // STORED mode: blocks are contiguous uncompressed data
    if (h->info.mode == 0x01) {
        uint64_t block_start = (uint64_t)block_index * h->info.block_size;
        uint64_t block_end = std::min(block_start + h->info.block_size, h->info.orig_size);
        info.compressed_size = (uint32_t)(block_end - block_start);
        info.original_size = info.compressed_size;
        info.file_offset = h->data_start + block_start;
        return true;
    }

    info = h->blocks[block_index];
    return true;
}

// Decompress a specific block to memory
std::vector<uint8_t> block_decompress(void* handle, uint32_t block_index) {
    if (!handle) return {};
    auto* h = static_cast<BlockFileHandle*>(handle);

    if (block_index >= h->info.block_count) return {};

    BlockInfo binfo;
    if (!block_get_info(handle, block_index, binfo)) return {};

    // Seek to block data
    if (fseek(h->fp, (long)binfo.file_offset, SEEK_SET) != 0) return {};

    // Read compressed block
    std::vector<uint8_t> cbuf(binfo.compressed_size);
    if (fread(cbuf.data(), 1, binfo.compressed_size, h->fp) != binfo.compressed_size) {
        return {};
    }

    // STORED mode: data is already uncompressed
    if (h->info.mode == 0x01) {
        return cbuf;
    }

    // Decompress based on version
    const uint8_t* bp = cbuf.data();
    uint8_t block_type = bp[0];

    if (h->info.version == 7) {
        // v7: LZ77 + delta + rANS order-0
        // (Implement v7 block decompression - same as in decompress_file)
        // For now, return empty (not implemented in this snippet)
        return {};
    }
    else if (h->info.version == 8) {
        // v8: BWT modes
        if (block_type == 0x01) {
            // STORED_RAW
            uint32_t raw_size = read_u32le(bp + 1);
            return std::vector<uint8_t>(bp + 5, bp + 5 + raw_size);
        }
        else if (block_type == 0x00) {
            // BWT + rANS order-0
            uint32_t primary_index = read_u32le(bp + 1);
            AnsTable tbl = ans_read_freqs(bp + 5);
            uint32_t sym_count = read_u32le(bp + 5 + 512);
            uint32_t enc_size = read_u32le(bp + 5 + 512 + 4);

            AnsDecoder dec;
            dec.init(bp + 5 + 512 + 8, enc_size);
            std::vector<uint8_t> rle(sym_count);
            for (uint32_t i = 0; i < sym_count; ++i)
                rle[i] = dec.get(tbl);

            auto mtf_buf = rle_zeros_decode(rle.data(), rle.size());
            auto bwt_buf = mtf_decode(mtf_buf.data(), mtf_buf.size());
            return bwt_decode(bwt_buf.data(), bwt_buf.size(), primary_index);
        }
        else if (block_type == 0x02 || block_type == 0x03) {
            // BWT + rANS order-1 or order-2
            uint32_t primary_index = read_u32le(bp + 1);
            auto ctx1_ptr = ans_ctx1_read(bp + 5);
            uint32_t sym_count = read_u32le(bp + 5 + 131072);
            uint32_t enc_size = read_u32le(bp + 5 + 131072 + 4);

            auto rle = ans_ctx1_decode(bp + 5 + 131072 + 8, enc_size, sym_count, *ctx1_ptr);
            auto mtf_buf = rle_zeros_decode(rle.data(), rle.size());
            auto bwt_buf = mtf_decode(mtf_buf.data(), mtf_buf.size());
            return bwt_decode(bwt_buf.data(), bwt_buf.size(), primary_index);
        }
    }
    else if (h->info.version == 9) {
        // v9: PPM
        if (block_type == 0x01) {
            // STORED_RAW
            uint32_t raw_size = read_u32le(bp + 1);
            return std::vector<uint8_t>(bp + 5, bp + 5 + raw_size);
        }
        else if (block_type == 0x00) {
            // PPM_CODED
            uint32_t sym_count = read_u32le(bp + 1);
            return ppm_decode(bp + 5, binfo.compressed_size - 5, sym_count);
        }
    }
    else if (h->info.version == 10) {
        // v10: CMIX
        if (block_type == 0x01) {
            // STORED_RAW
            uint32_t raw_size = read_u32le(bp + 1);
            return std::vector<uint8_t>(bp + 5, bp + 5 + raw_size);
        }
        else if (block_type == 0x00) {
            // CMIX_CODED
            uint32_t sym_count = read_u32le(bp + 1);
            return cmix_decode(bp + 5, binfo.compressed_size - 5, sym_count);
        }
    }

    return {};  // Unknown format
}

// Close block file handle
void block_close(void* handle) {
    if (!handle) return;
    auto* h = static_cast<BlockFileHandle*>(handle);
    if (h->fp) fclose(h->fp);
    delete h;
}
