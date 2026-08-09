#include "algorithm/format.h"

#include <gtest/gtest.h>

#include <cstring>

using namespace pmem;
using namespace pmem::internal;

TEST(Format, ParseTypes) {
    ValueType t;
    EXPECT_TRUE(parse_value_type("byte", t));
    EXPECT_EQ(t, ValueType::Byte);
    EXPECT_TRUE(parse_value_type("word", t));
    EXPECT_TRUE(parse_value_type("dword", t));
    EXPECT_TRUE(parse_value_type("qword", t));
    EXPECT_TRUE(parse_value_type("float", t));
    EXPECT_TRUE(parse_value_type("double", t));
    EXPECT_FALSE(parse_value_type("int", t));
}

TEST(Format, TypeSizes) {
    EXPECT_EQ(value_type_size(ValueType::Byte), 1u);
    EXPECT_EQ(value_type_size(ValueType::Word), 2u);
    EXPECT_EQ(value_type_size(ValueType::Dword), 4u);
    EXPECT_EQ(value_type_size(ValueType::Qword), 8u);
    EXPECT_EQ(value_type_size(ValueType::Float), 4u);
    EXPECT_EQ(value_type_size(ValueType::Double), 8u);
}

TEST(Format, DwordValue) {
    const uint8_t data[] = {0x44, 0x33, 0x22, 0x11};
    std::string s;
    EXPECT_TRUE(format_value(data, 4, ValueType::Dword, s));
    EXPECT_EQ(s, "0x11223344");
}

TEST(Format, ByteValue) {
    const uint8_t data[] = {0xCC};
    std::string s;
    EXPECT_TRUE(format_value(data, 1, ValueType::Byte, s));
    EXPECT_EQ(s, "0xCC");
}

TEST(Format, FloatValue) {
    uint8_t data[4];
    float v = 3.5f;
    memcpy(data, &v, 4);
    std::string s;
    EXPECT_TRUE(format_value(data, 4, ValueType::Float, s));
    EXPECT_EQ(s, "3.5");
}

TEST(Format, DoubleValue) {
    uint8_t data[8];
    double v = 2.0;
    memcpy(data, &v, 8);
    std::string s;
    EXPECT_TRUE(format_value(data, 8, ValueType::Double, s));
    EXPECT_EQ(s, "2");
}

TEST(Format, TooShort) {
    const uint8_t data[] = {0x01, 0x02};
    std::string s;
    EXPECT_FALSE(format_value(data, 2, ValueType::Qword, s));
}
