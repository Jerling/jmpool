#ifndef __JMP_ALLOCATOR_H_
#define __JMP_ALLOCATOR_H_

#include "jmp_inner_inc.h"

const size_t BIGSZ = 4096;
const size_t NUMS = 16;
const size_t MINSZ = 8;
const size_t MINFSZ = 8;
constexpr int small_node_nums(size_t sz) {
  int x = 0;
  while ((sz >>= 1)) ++x;
  return x;
}

namespace jmp {
namespace details {

/* 移动指针 */
struct __list_next_base {
  __list_next_base() : next(nullptr) {}
  __list_next_base *next;
};
/* 内存节点 */
struct __mem_node : __list_next_base {
  __mem_node() : size(0) {}
  size_t size;
};

class small_alloc;
/* 大块内存配置器 */
class big_alloc {
 public:
  big_alloc(std::shared_ptr<small_alloc> alloc) : _salloc(alloc) {
    _log = jlog::logger::new_logger("jmp", {1}, jlog::log_level_t::debug);
  }
  big_alloc(std::shared_ptr<small_alloc> alloc,
            std::shared_ptr<jlog::logger> log)
      : _salloc(alloc), _log(log) {}
  /* 每次分配两个一样大的内存块 */
  void *allocate(size_t sz);
  void deallocate(void *ptr, size_t sz) {
    ((big_node *)(ptr))->size = sz;
    add_to_free((big_node *)ptr);
  }

 private:
  using big_node = __mem_node;
  using self = big_alloc;
  /* 添加到空闲链表 */
  void add_to_free(big_node *node);
  /* 从空闲块中找 */
  void *malloc_chrunk(size_t sz);
  /* 内存池获取失败，从系统获取
   * 获取策略： 固定 + 动态
   * 固定：每次申请 2 个 sz
   * 动态：根据当前堆的大小动态调整，每次多申请 heap_size / BIGSZ 的大小 */
  void *bmalloc(size_t sz);
  void *oom_malloc(size_t sz);

 private:
  big_node free[1] = {};
  size_t heap_size = 0;
  std::shared_ptr<jlog::logger> _log;
  std::shared_ptr<small_alloc> _salloc;
};

/* 小块内存配置器 */
class small_alloc {
  friend big_alloc;

 public:
  small_alloc() {
    _log = jlog::logger::new_logger("jmp", {1}, jlog::log_level_t::debug);
  }
  small_alloc(std::shared_ptr<jlog::logger> log) { _log = log; }
  void *allocate(size_t sz);
  void deallocate(void *ptr, size_t sz) {
    auto tsz = sz;
    if (0 != (sz & (sz - 1))) auto tsz = (up_to_two_power(sz) >> 1);
    add_to_free((small_node *)ptr, tsz);
    adjust_to_fit((char *)ptr + tsz, get_index(sz - tsz), sz - tsz);
  }

 private:
  using small_node = __list_next_base;
  using self = small_alloc;
  /* 上调申请字节为 2 的次幂 */
  size_t up_to_two_power(size_t sz) {
    if (sz < MINSZ) return MINSZ;
    if (0 == (sz & (sz - 1))) return sz;
    int t = get_index(sz) + small_node_nums(MINSZ);
    return (1 << (t + 1));
  }
  /* 根据 size 获取所在的空闲链表的索引 */
  size_t get_index(size_t sz) {
    int x = 0;
    while ((sz >>= 1)) ++x;
    return x - small_node_nums(MINSZ);
  }
  /* 添加到空闲链表 */
  void add_to_free(small_node *node, size_t sz);
  void add_to_free(small_node *node, uint16_t idx) {
    add_to_free(node, (size_t)(1 << (idx + small_node_nums(MINSZ))));
  }
  /* 将连续 n-1 块添加到空闲块 */
  void add_to_free(void *mem, size_t sz, uint16_t n);
  /* 内存池获取失败，从系统获取
   * 获取策略： 固定 + 动态
   * 固定：每次申请 NUMS 个 sz
   * 动态：根据当前堆的大小动态调整，每次多申请 heap_size / BIGSZ 的大小 */
  void *smalloc(size_t tsz);
  /* 从较大空闲内存中获取内存 */
  void *malloc_chrunk(size_t sz, uint16_t &n);
  /* 分配失败 */
  void *oom_malloc(size_t sz);
  /* 当分配的空间不是 2 的次幂时，自动将多余的部份添加到小空闲内存块后 */
  void adjust_to_fit(void *, uint16_t, size_t);

 private:
  small_node free[small_node_nums(BIGSZ / MINSZ) + 1] = {};
  size_t heap_size = 0;
  std::shared_ptr<jlog::logger> _log;
};  // namespace details
}  // namespace details

class jmp_allocator {
 public:
  jmp_allocator() {
    _log = jlog::logger::new_logger("jmp", {1}, jlog::log_level_t::debug);
    init();
  }
  jmp_allocator(std::shared_ptr<jlog::logger> log) {
    _log = log;
    init();
  }
  void *allocate(std::size_t sz);
  void deallocate(void *ptr, size_t sz);

 private:
  void init() {
    DEBUG_DTL(_log, "The minsz = %d, bigsz = %d, the nodes is %d\n", MINSZ,
              BIGSZ, small_node_nums(BIGSZ / MINSZ) + 1);
    smem = std::make_shared<salloc>(_log);
    bmem = std::make_shared<balloc>(smem, _log);
  }
  size_t up_to_minsz_times(size_t sz) {
    return ((sz + MINSZ - 1) >> small_node_nums(MINSZ))
           << small_node_nums(MINSZ);
  }

 private:
  using balloc = details::big_alloc;
  using salloc = details::small_alloc;
  std::shared_ptr<jlog::logger> _log;
  std::shared_ptr<salloc> smem;
  std::shared_ptr<balloc> bmem;
};
}  // namespace jmp

#endif  // __JMP_ALLOCATOR_H_
