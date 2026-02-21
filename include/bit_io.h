#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>

// -----------------------------------------------------------------------------
// BitWriter  —  write individual bits into a byte buffer
// -----------------------------------------------------------------------------
// Usage:
//   BitWriter bw;
//   bw.write_bit(1);
//   bw.write_bits(0b1011, 4);   // write 4 bits, MSB first
//   bw.flush();                 // pad final byte with zeros
//   std::vector<uint8_t> out = bw.data();
// -----------------------------------------------------------------------------
class BitWriter {
public:
    void write_bit(int bit) {
        buf_ = (buf_ << 1) | (bit & 1);
        ++bits_;
        if (bits_ == 8) {
            out_.push_back(buf_);
            buf_  = 0;
            bits_ = 0;
        }
    }

    // Write `count` bits from `value`, MSB first
    void write_bits(uint64_t value, int count) {
        for (int i = count - 1; i >= 0; --i)
            write_bit((value >> i) & 1);
    }

    // Flush any partial byte (zero-padded on the right)
    void flush() {
        if (bits_ > 0) {
            buf_ <<= (8 - bits_);
            out_.push_back(buf_);
            buf_  = 0;
            bits_ = 0;
        }
    }

    const std::vector<uint8_t>& data() const { return out_; }
    size_t bit_count() const { return out_.size() * 8 - (bits_ == 0 ? 0 : (8 - bits_)); }
    // total valid bits written (before flush)
    size_t bits_written() const { return out_.size() * 8 + bits_; }

private:
    uint8_t              buf_  = 0;
    int                  bits_ = 0;
    std::vector<uint8_t> out_;
};


// -----------------------------------------------------------------------------
// BitReader  —  read individual bits from a byte buffer
// -----------------------------------------------------------------------------
// Usage:
//   BitReader br(data.data(), data.size(), total_bits);
//   int bit = br.read_bit();
//   uint64_t val = br.read_bits(4);
// -----------------------------------------------------------------------------
class BitReader {
public:
    BitReader(const uint8_t* data, size_t byte_len, size_t total_bits)
        : data_(data), byte_len_(byte_len), total_bits_(total_bits) {}

    int read_bit() {
        if (bits_read_ >= total_bits_)
            throw std::out_of_range("BitReader: read past end of bitstream");
        if (bit_pos_ == 0) {
            if (byte_pos_ >= byte_len_)
                throw std::out_of_range("BitReader: buffer exhausted");
            cur_byte_ = data_[byte_pos_++];
            bit_pos_  = 8;
        }
        int bit = (cur_byte_ >> (--bit_pos_)) & 1;
        ++bits_read_;
        return bit;
    }

    uint64_t read_bits(int count) {
        uint64_t val = 0;
        for (int i = 0; i < count; ++i)
            val = (val << 1) | read_bit();
        return val;
    }

    bool at_end() const { return bits_read_ >= total_bits_; }
    size_t bits_remaining() const { return total_bits_ - bits_read_; }

private:
    const uint8_t* data_;
    size_t         byte_len_;
    size_t         total_bits_;
    size_t         byte_pos_  = 0;
    size_t         bits_read_ = 0;
    uint8_t        cur_byte_  = 0;
    int            bit_pos_   = 0;
};
