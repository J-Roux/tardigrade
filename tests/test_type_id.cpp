#include <catch2/catch_test_macros.hpp>

#include <tartigrada/type_id.hpp>
#include <tartigrada/message.hpp>

using namespace tartigrada;

struct foo_t {};
struct bar_t {};

struct foo_msg_t : message_t<foo_msg_t> {};
struct bar_msg_t : message_t<bar_msg_t> {};

TEST_CASE("same type always produces the same id", "[type_id]")
{
    STATIC_REQUIRE(type_id<foo_t>() == type_id<foo_t>());
    STATIC_REQUIRE(type_id<int>()   == type_id<int>());
}

TEST_CASE("different types produce different ids", "[type_id]")
{
    STATIC_REQUIRE(type_id<foo_t>() != type_id<bar_t>());
    STATIC_REQUIRE(type_id<int>()   != type_id<float>());
}

TEST_CASE("message_t::type_id matches type_id<T>()", "[type_id]")
{
    STATIC_REQUIRE(foo_msg_t::type_id == type_id<foo_msg_t>());
    STATIC_REQUIRE(bar_msg_t::type_id == type_id<bar_msg_t>());
}

TEST_CASE("message_t::type_id differs between message types", "[type_id]")
{
    STATIC_REQUIRE(foo_msg_t::type_id != bar_msg_t::type_id);
}

TEST_CASE("get_id() matches static type_id", "[type_id]")
{
    foo_msg_t msg;
    REQUIRE(msg.get_id() == foo_msg_t::type_id);
}
