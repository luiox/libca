#ifndef LIBCA_BASE_RESULT_HPP
#define LIBCA_BASE_RESULT_HPP

namespace ca {


template <typename T, typename E>
class Result {
private:
    T value_;
    E error_;

    Result(T value, E error) : value_(value), error_(error) {}

public:

    static Result<T, E> Ok(T value) {
        return Result<T, E>(value, nullptr);
    }

    static Result<T, E> Err(E error) {
        return Result<T, E>(nullptr, error);
    }

    bool isOk() const {
        return error_ != nullptr;
    }

    bool isErr() const {
        return error_ != nullptr;
    }

    T unwrap() const {
        return value_;
    }

    T except() const {
        return value_;
    }
    
};

}

#endif // !LIBCA_BASE_RESULT_HPP
