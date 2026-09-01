#include <gtest/gtest.h>

#include "libca/core/option.hpp"
#include "libca/core/result.hpp"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace ca::core;

// ============================================================================
// 基础构造与判别
// ============================================================================

TEST(OptionTest, DefaultAndNoneAreEmpty)
{
    Option<int> a;
    EXPECT_TRUE(a.is_none());
    EXPECT_FALSE(a.is_some());
    EXPECT_FALSE(static_cast<bool>(a));

    Option<int> b = None;
    EXPECT_TRUE(b.is_none());

    a = None;
    EXPECT_TRUE(a.is_none());
}

TEST(OptionTest, SomeCarriesValue)
{
    Option<int> x = Some(42);
    ASSERT_TRUE(x.is_some());
    EXPECT_EQ(x.unwrap(), 42);

    Option<std::string> s = Some(std::string("hello"));
    EXPECT_EQ(s.unwrap(), "hello");
}

TEST(OptionTest, InPlaceConstructor)
{
    Option<std::string> s(std::in_place, 3, 'a');
    ASSERT_TRUE(s.is_some());
    EXPECT_EQ(s.unwrap(), "aaa");

    Option<std::unique_ptr<int>> p(std::in_place, new int(7));
    ASSERT_TRUE(p.is_some());
    EXPECT_EQ(*p.unwrap(), 7);
}

// ============================================================================
// unwrap 家族
// ============================================================================

TEST(OptionTest, UnwrapOrProvidesDefault)
{
    Option<int> some = Some(5);
    Option<int> none;

    EXPECT_EQ(some.unwrap_or(99), 5);
    EXPECT_EQ(none.unwrap_or(99), 99);
    // 移动语义：Some 的值被移出。
    Option<std::string> s = Some(std::string("payload"));
    EXPECT_EQ(std::move(s).unwrap_or(std::string("x")), "payload");
}

TEST(OptionTest, UnwrapOrElseLazilyEvaluates)
{
    bool called  = false;
    auto factory = [&called]() {
        called = true;
        return -1;
    };
    Option<int> some = Some(1);
    EXPECT_EQ(some.unwrap_or_else(factory), 1);
    EXPECT_FALSE(called);   // Some 时不调用默认工厂

    Option<int> none;
    EXPECT_EQ(none.unwrap_or_else(factory), -1);
    EXPECT_TRUE(called);
}

TEST(OptionTest, UnwrapOrDefault)
{
    Option<int> none;
    EXPECT_EQ(none.unwrap_or_default(), 0);

    Option<std::string> s = Some(std::string("v"));
    EXPECT_EQ(s.unwrap_or_default(), "v");
}

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4722)   // 测试 death 路径的 terminate 不可达返回
#endif

// unwrap/expect 在 None 上 std::terminate（与 Result::unwrap 失败行为一致）。
// 不在 CI 里做 death test（平台差异大），这里只验证 Some 路径可正常取值。
TEST(OptionTest, ExpectReturnsValueOnSome)
{
    Option<int> x = Some(3);
    EXPECT_EQ(x.expect("should have value"), 3);

    const Option<int> cx = Some(4);
    EXPECT_EQ(cx.expect("const some"), 4);

    Option<std::string> s = Some(std::string("m"));
    EXPECT_EQ(std::move(s).expect("moved"), "m");
}

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

// ============================================================================
// 指针式访问
// ============================================================================

TEST(OptionTest, ArrowAndDerefAccess)
{
    Option<std::string> s = Some(std::string("abc"));
    ASSERT_TRUE(s.is_some());
    EXPECT_EQ(s->size(), 3u);
    EXPECT_EQ((*s).size(), 3u);

    const Option<std::string> cs = Some(std::string("xyz"));
    EXPECT_EQ(cs->size(), 3u);
}

// ============================================================================
// 组合子
// ============================================================================

TEST(OptionTest, MapTransformsValue)
{
    Option<int> x      = Some(2);
    auto        mapped = x.map([](int v) { return v * 10; });
    ASSERT_TRUE(mapped.is_some());
    EXPECT_EQ(mapped.unwrap(), 20);

    Option<int> none;
    auto        mapped_none = none.map([](int v) { return v * 10; });
    EXPECT_TRUE(mapped_none.is_none());

    // map 改变类型。
    Option<int> n         = Some(7);
    auto        as_string = n.map([](int v) { return std::to_string(v); });
    ASSERT_TRUE(as_string.is_some());
    EXPECT_EQ(as_string.unwrap(), "7");
}

TEST(OptionTest, MapMovesValue)
{
    Option<std::unique_ptr<int>> p = Some(std::make_unique<int>(9));
    auto extracted                 = std::move(p).map([](std::unique_ptr<int> v) { return *v; });
    ASSERT_TRUE(extracted.is_some());
    EXPECT_EQ(extracted.unwrap(), 9);
}

TEST(OptionTest, AndThenChainsOptions)
{
    auto half = [](int v) -> Option<int> { return v % 2 == 0 ? Some(v / 2) : Option<int>{}; };

    Option<int> a       = Some(20);
    auto        chained = a.and_then(half).and_then(half).and_then(half);
    // 20 → 10 → 5 → None（5 是奇数）。
    EXPECT_TRUE(chained.is_none());

    Option<int> b        = Some(16);
    auto        ok_chain = b.and_then(half).and_then(half);
    ASSERT_TRUE(ok_chain.is_some());
    EXPECT_EQ(ok_chain.unwrap(), 4);

    Option<int> none;
    EXPECT_TRUE(none.and_then(half).is_none());
}

TEST(OptionTest, OrElseProvidesAlternative)
{
    auto fallback = []() { return Some<int>(-1); };

    Option<int> none;
    auto        recovered = none.or_else(fallback);
    ASSERT_TRUE(recovered.is_some());
    EXPECT_EQ(recovered.unwrap(), -1);

    Option<int> some = Some(1);
    auto        kept = some.or_else(fallback);
    ASSERT_TRUE(kept.is_some());
    EXPECT_EQ(kept.unwrap(), 1);
}

TEST(OptionTest, TakeMovesOutAndResets)
{
    Option<std::string> s     = Some(std::string("data"));
    auto                taken = s.take();
    ASSERT_TRUE(taken.is_some());
    EXPECT_EQ(taken.unwrap(), "data");
    EXPECT_TRUE(s.is_none());   // 原对象变 None

    Option<int> none;
    EXPECT_TRUE(none.take().is_none());
}

// ============================================================================
// Result 桥接
// ============================================================================

TEST(OptionTest, OkOrConvertsToResult)
{
    Option<int> some = Some(10);
    auto        ok   = some.ok_or(std::string("missing"));
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(ok.unwrap(), 10);

    Option<int> none;
    auto        err = none.ok_or(std::string("missing"));
    ASSERT_TRUE(err.is_err());
    EXPECT_EQ(err.unwrap_err(), "missing");
}

TEST(OptionTest, ResultOkAndErrConvertBack)
{
    Result<int, std::string> ok_result = Ok(3);
    auto                     as_option = ok_result.ok();
    ASSERT_TRUE(as_option.is_some());
    EXPECT_EQ(as_option.unwrap(), 3);
    EXPECT_TRUE(ok_result.err().is_none());

    Result<int, std::string> err_result = Err(std::string("boom"));
    EXPECT_TRUE(err_result.ok().is_none());
    auto err_opt = err_result.err();
    ASSERT_TRUE(err_opt.is_some());
    EXPECT_EQ(err_opt.unwrap(), "boom");
}

TEST(OptionTest, ResultOkMovesValue)
{
    Result<std::unique_ptr<int>, std::string> r   = Ok(std::make_unique<int>(5));
    auto                                      opt = std::move(r).ok();
    ASSERT_TRUE(opt.is_some());
    EXPECT_EQ(*opt.unwrap(), 5);
}

TEST(OptionTest, ResultVoidErrStillWorks)
{
    // Result<void, E> 没有 ok()（Option<void> 无意义），但 err() 可用。
    Result<void, int> r = Err(7);
    auto              e = r.err();
    ASSERT_TRUE(e.is_some());
    EXPECT_EQ(e.unwrap(), 7);
}

// ============================================================================
// 比较 / 交换 / hash
// ============================================================================

TEST(OptionTest, EqualitySemantics)
{
    EXPECT_EQ(Some(1), Some(1));
    EXPECT_NE(Some(1), Some(2));
    EXPECT_NE((Option<int>{}), Some(1));

    Option<int> a, b;
    EXPECT_EQ(a, b);          // 两个 None 相等
    EXPECT_TRUE(a == None);   // 与哨兵比较
    EXPECT_TRUE(Some(1) != None);
}

TEST(OptionTest, SwapExchangesContents)
{
    Option<int> a = Some(1);
    Option<int> b = Some(2);
    a.swap(b);
    EXPECT_EQ(a.unwrap(), 2);
    EXPECT_EQ(b.unwrap(), 1);

    Option<int> c = Some(3);
    Option<int> d;
    swap(c, d);   // 非成员版本
    EXPECT_TRUE(c.is_none());
    EXPECT_EQ(d.unwrap(), 3);
}

TEST(OptionTest, HashWorksInUnorderedContainer)
{
    std::unordered_set<Option<int>> set;
    set.insert(Some(1));
    set.insert(Some(1));   // 重复
    set.insert(Some(2));
    set.insert(Option<int>{});

    EXPECT_EQ(set.size(), 3u);   // Some(1) 去重
    EXPECT_TRUE(set.count(Some(1)) > 0);
    EXPECT_TRUE(set.count(Option<int>{}) > 0);
    EXPECT_TRUE(set.count(Some(3)) == 0);
}

// ============================================================================
// move-only 类型与生命周期
// ============================================================================

TEST(OptionTest, MoveOnlyValue)
{
    Option<std::unique_ptr<int>> p = Some(std::make_unique<int>(11));
    ASSERT_TRUE(p.is_some());
    std::unique_ptr<int> taken = std::move(p).unwrap();
    EXPECT_EQ(*taken, 11);

    // 拷贝不可用（编译期约束），移动赋值可用。
    Option<std::unique_ptr<int>> q;
    q = Some(std::make_unique<int>(22));
    EXPECT_EQ(*q.unwrap(), 22);
}

TEST(OptionTest, ReassignmentTransitionsStates)
{
    Option<int> x;
    x = Some(1);
    ASSERT_TRUE(x.is_some());
    EXPECT_EQ(x.unwrap(), 1);
    x = None;
    EXPECT_TRUE(x.is_none());
    x = Some(2);
    EXPECT_EQ(x.unwrap(), 2);
}

namespace {
// 构造/析构计数：验证 None 路径不构造多余对象。
struct Counter
{
    static int constructed;
    static int destructed;
    int        value;
    explicit Counter(int v)
        : value(v)
    {
        ++constructed;
    }
    Counter(const Counter& o)
        : value(o.value)
    {
        ++constructed;
    }
    Counter(Counter&& o) noexcept
        : value(o.value)
    {
        ++constructed;
    }
    ~Counter() { ++destructed; }
};
int Counter::constructed = 0;
int Counter::destructed  = 0;
}   // namespace

TEST(OptionTest, ConstructionDestructionAreBalanced)
{
    Counter::constructed = 0;
    Counter::destructed  = 0;
    {
        Option<Counter> a = Some(Counter(1));   // 移动构造 1 次（+Some 工厂转移）
        Option<Counter> b;                      // None：不构造
        auto            mapped = a.map([](const Counter& c) { return c.value * 2; });
        ASSERT_TRUE(mapped.is_some());
        EXPECT_EQ(mapped.unwrap(), 2);
    }
    EXPECT_EQ(Counter::constructed, Counter::destructed);
    EXPECT_GT(Counter::constructed, 0);
}
