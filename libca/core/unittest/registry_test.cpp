#include <gtest/gtest.h>

#include "libca/core/registry.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace ca::core::test {
namespace {

// ============================================================================
// 测试产品体系
// ============================================================================

class Animal {
public:
    virtual ~Animal() = default;
    virtual int legs() const = 0;
};

class Dog : public Animal {
public:
    int legs() const override { return 4; }
};

class Duck : public Animal {
public:
    explicit Duck(int extra) : extra_(extra) {}
    int legs() const override { return 2 + extra_; }

private:
    int extra_;
};

using AnimalRegistry = Registry<std::string, Animal>;

// 文件级静态注册器对象：静态初始化阶段（main 之前）完成注册，
// 是 Doxygen 中"静态自注册惯用法"的真实可运行示例。
const bool g_registered_dog = AnimalRegistry::register_factory(
    "dog",
    []() -> std::unique_ptr<Animal> { return std::make_unique<Dog>(); }).is_ok();

// RAII 注销器：用例结束时清理自己注册的键，保证用例间互不污染
// （注册表是 <Key, Base, Args...> 维度的进程级 static 状态）。
class ScopedRegistration {
public:
    explicit ScopedRegistration(std::string key) : key_(std::move(key)) {}
    ~ScopedRegistration() { AnimalRegistry::unregister_factory(key_); }

    ScopedRegistration(const ScopedRegistration&) = delete;
    ScopedRegistration& operator=(const ScopedRegistration&) = delete;

private:
    std::string key_;
};

} // namespace

TEST(RegistryTest, StaticRegistrationBeforeMain) {
    // 文件级静态注册器对象在 main 之前注册成功，且创建可用。
    EXPECT_TRUE(g_registered_dog);
    auto animal = AnimalRegistry::create("dog");
    ASSERT_TRUE(animal.is_ok());
    EXPECT_EQ(std::move(animal).unwrap()->legs(), 4);
}

TEST(RegistryTest, RegisterDuplicateKeyFailsWithoutOverwrite) {
    ScopedRegistration guard("dup");
    EXPECT_TRUE(AnimalRegistry::register_factory(
        "dup", []() -> std::unique_ptr<Animal> { return std::make_unique<Dog>(); }).is_ok());
    // 重复 key：返回 ALREADY_EXISTS，且原工厂不被覆盖。
    auto again = AnimalRegistry::register_factory(
        "dup", []() -> std::unique_ptr<Animal> { return nullptr; });
    ASSERT_TRUE(again.is_err());
    EXPECT_EQ(again.code(), StatusCode::ALREADY_EXISTS);

    auto animal = AnimalRegistry::create("dup");
    ASSERT_TRUE(animal.is_ok());
    EXPECT_EQ(std::move(animal).unwrap()->legs(), 4);
}

TEST(RegistryTest, CreateUnknownKeyReturnsNotFound) {
    auto missing = AnimalRegistry::create("no_such_key");
    ASSERT_TRUE(missing.is_err());
    EXPECT_EQ(missing.unwrap_err().code(), StatusCode::NOT_FOUND);
}

TEST(RegistryTest, UnregisterRemovesFactory) {
    EXPECT_TRUE(AnimalRegistry::register_factory(
        "temp", []() -> std::unique_ptr<Animal> { return std::make_unique<Dog>(); }).is_ok());
    EXPECT_TRUE(AnimalRegistry::contains("temp"));

    EXPECT_TRUE(AnimalRegistry::unregister_factory("temp"));
    EXPECT_FALSE(AnimalRegistry::contains("temp"));
    EXPECT_TRUE(AnimalRegistry::create("temp").is_err());
    // 重复注销返回 false，不报错。
    EXPECT_FALSE(AnimalRegistry::unregister_factory("temp"));
}

TEST(RegistryTest, KeysReturnsSortedRegisteredKeys) {
    ScopedRegistration guard_b("zz_late");
    ScopedRegistration guard_a("aa_early");
    ASSERT_TRUE(AnimalRegistry::register_factory(
        "zz_late", []() -> std::unique_ptr<Animal> { return std::make_unique<Dog>(); }).is_ok());
    ASSERT_TRUE(AnimalRegistry::register_factory(
        "aa_early", []() -> std::unique_ptr<Animal> { return std::make_unique<Dog>(); }).is_ok());

    const auto key_list = AnimalRegistry::keys();
    const auto first = std::find(key_list.begin(), key_list.end(), std::string("aa_early"));
    const auto last = std::find(key_list.begin(), key_list.end(), std::string("zz_late"));
    ASSERT_NE(first, key_list.end());
    ASSERT_NE(last, key_list.end());
    // std::map 有序存储：先注册的 aa_early 排在 zz_late 之前。
    EXPECT_LT(first, last);
}

TEST(RegistryTest, FactoryWithArguments) {
    using DuckRegistry = Registry<std::string, Animal, int>;
    EXPECT_TRUE(DuckRegistry::register_factory(
        "duck",
        [](int extra) -> std::unique_ptr<Animal> { return std::make_unique<Duck>(extra); }).is_ok());

    auto duck = DuckRegistry::create("duck", 3);
    ASSERT_TRUE(duck.is_ok());
    EXPECT_EQ(std::move(duck).unwrap()->legs(), 5);
    EXPECT_TRUE(DuckRegistry::unregister_factory("duck"));
}

TEST(RegistryTest, NonStringKey) {
    using NumericRegistry = Registry<int, Animal>;
    EXPECT_TRUE(NumericRegistry::register_factory(
        42, []() -> std::unique_ptr<Animal> { return std::make_unique<Dog>(); }).is_ok());
    EXPECT_TRUE(NumericRegistry::contains(42));
    auto animal = NumericRegistry::create(42);
    ASSERT_TRUE(animal.is_ok());
    EXPECT_EQ(std::move(animal).unwrap()->legs(), 4);
    EXPECT_TRUE(NumericRegistry::unregister_factory(42));
}

TEST(RegistryTest, EmptyFactoryRejected) {
    ScopedRegistration guard("null_factory");
    auto rejected = AnimalRegistry::register_factory("null_factory", nullptr);
    ASSERT_TRUE(rejected.is_err());
    EXPECT_EQ(rejected.code(), StatusCode::INVALID_ARGUMENT);
    EXPECT_FALSE(AnimalRegistry::contains("null_factory"));
}

} // namespace ca::core::test
