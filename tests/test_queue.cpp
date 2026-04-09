#include <catch2/catch_test_macros.hpp>

#include <tartigrada/node.hpp>
#include <tartigrada/queue.hpp>

using namespace tartigrada;

struct item : node<item>
{
    int value;
    explicit item(int v) : value{ v } {}
};

TEST_CASE("empty queue", "[queue]")
{
    queue<item> q;
    REQUIRE(q.empty());
    REQUIRE(q.length() == 0);
    REQUIRE(q.front() == nullptr);
}

TEST_CASE("push_back appends in order", "[queue]")
{
    queue<item> q;
    item a{ 1 }, b{ 2 }, c{ 3 };

    q.push_back(&a);
    q.push_back(&b);
    q.push_back(&c);

    REQUIRE_FALSE(q.empty());
    REQUIRE(q.length() == 3);
    REQUIRE(q.front()->value == 1);
}

TEST_CASE("pop_front removes head in FIFO order", "[queue]")
{
    queue<item> q;
    item a{ 10 }, b{ 20 }, c{ 30 };

    q.push_back(&a);
    q.push_back(&b);
    q.push_back(&c);

    REQUIRE(q.front()->value == 10);
    q.pop_front();
    REQUIRE(q.front()->value == 20);
    q.pop_front();
    REQUIRE(q.front()->value == 30);
    q.pop_front();
    REQUIRE(q.empty());
}

TEST_CASE("range-for yields elements in FIFO order", "[queue]")
{
    queue<item> q;
    item a{ 1 }, b{ 2 }, c{ 3 };
    q.push_back(&a);
    q.push_back(&b);
    q.push_back(&c);

    int expected[] = { 1, 2, 3 };
    int i = 0;
    for (auto* it : q)
        REQUIRE(it->value == expected[i++]);
    REQUIRE(i == 3);
}

TEST_CASE("single element push and pop", "[queue]")
{
    queue<item> q;
    item a{ 42 };

    q.push_back(&a);
    REQUIRE(q.length() == 1);
    REQUIRE(q.front()->value == 42);

    q.pop_front();
    REQUIRE(q.empty());
}
