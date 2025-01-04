#ifndef LIBCA_UTILITY_NOCOPYABLE_H
#define LIBCA_UTILITY_NOCOPYABLE_H

namespace libca::utility {

class nocopyable
{
protected:
    nocopyable() {}
    ~nocopyable() {}

private:
    nocopyable(const nocopyable&);
    nocopyable& operator=(const nocopyable&);
};

}   // namespace libca::utility

#endif   // LIBCA_UTILITY_NOCOPYABLE_H