#ifndef __JMP_CONSTRUCT_H_
#define __JMP_CONSTRUCT_H_

#include <new>
#include <type_traits>

namespace jmp {
namespace details {

template <typename forward_iterator>
inline void __destory(forward_iterator first, forward_iterator last,
                      std::true_type) {}
template <typename forward_iterator>
inline void __destory(forward_iterator first, forward_iterator last,
                      std::false_type) {
  for (; first < last; ++first) destory(&*first);
}

template <typename T1, typename T2>
inline void _construct(T1 *ptr, const T2 &value) {
  new (ptr) T1(value);
}

template <typename T>
inline void _destory(T *ptr) {
  if (nullptr != ptr) ptr->~T();
}

template <typename forward_iterator>
inline void _destory(forward_iterator first, forward_iterator last) {
  __destory(first, last,
            std::is_trivially_destructible<forward_iterator>::value);
}

inline void _destory(char *, char *){};
inline void _destory(wchar_t *, wchar_t *){};

}  // namespace details
}  // namespace jmp

#endif  // __JMP_CONSTRUCT_H_
