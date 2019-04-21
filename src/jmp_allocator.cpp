#include "jmp_allocator.h"

namespace jmp {
namespace details {

void big_alloc::add_to_free(big_node *node) {
  auto pp = (big_node *)&free[0];
  auto p = (big_node *)free->next;
  while (nullptr != p && node > p) {
    pp = p;
    p = (big_node *)p->next;
  }
  if ((void *)(node->size + (char *)node) == (void *)p) {
    DEBUG_DTL(_log, "big_alloc merge %p:%d and %p:%d", node, node->size, p,
              p->size);
    node->size += p->size;
    node->next = p->next;
  } else {
    node->next = p;
  }
  pp->next = node;
}

void *big_alloc::allocate(size_t sz) {
  DEBUG_DTL(_log, "Try allocate %d with big_alloc", sz);
  void *result = nullptr;
  result = malloc_chrunk(sz);
  if (nullptr == result) {
    result = bmalloc(sz);
  }
  if (nullptr == result) {
    result = oom_malloc(sz << 1);
  }
  return result;
}

void *big_alloc::bmalloc(size_t sz) {
  void *result = malloc((sz << 1) + (heap_size >> small_node_nums(BIGSZ)));
  heap_size += (sz << 1) + (heap_size >> small_node_nums(BIGSZ));
  auto node = (big_node *)((char *)result + sz);
  node->size = sz + (heap_size >> small_node_nums(BIGSZ));
  add_to_free((big_node *)result);
  return result;
}

void *big_alloc::oom_malloc(size_t sz) { return nullptr; }

void small_alloc::add_to_free(small_node *node, size_t sz) {
  DEBUG_DTL(_log, "Add node address %p on free[%d]", node, get_index(sz));
  auto pp = &free[get_index(sz)];
  auto p = pp->next;
  while (p != nullptr && p < node) {
    pp = p;
    p = p->next;
  }
  node->next = pp->next;
  pp->next = node;
  return;
}

void *big_alloc::malloc_chrunk(size_t sz) {
  void *result = nullptr;
  if (nullptr != free[0].next) {
    DEBUG_DTL(_log, "Try to select a chrunk from free");
    auto pp = (big_node *)&free[0];
    auto p = (big_node *)pp->next;
    while (nullptr != p) {
      if (p->size < sz) {
        pp = p;
        p = (big_node *)p->next;
        continue;
      }
      result = (void *)p;
      if (p->size == sz) {
        /* 刚好合适 */
        pp->next = p->next;
      } else {
        /* 截取一部分分给区间，剩余的部份根据大小决定分给小缓冲池还是留给自己的空闲块
         */
        auto node = (big_node *)((char *)p + sz);
        size_t rsz = 0;
        if ((rsz = p->size - sz) < BIGSZ) {
          _salloc->adjust_to_fit(node, small_node_nums(BIGSZ), rsz);
          pp->next = p->next;
        } else {
          node->size = rsz;
          node->next = p->next;
          pp->next = node;
        }
      }
    }
  }
  return result; /* 空闲块中找到 */
}

void small_alloc::add_to_free(void *mem, size_t sz, uint16_t n) {
  auto node = (small_node *)((char *)mem + sz);
  DEBUG_DTL(_log, "Add %d nodes address %p on free[%d]", n - 1, node,
            get_index(sz));
  auto pp = &free[get_index(sz)];
  auto p = pp->next;
  while (p != nullptr && p < node) {
    pp = p;
    p = p->next;
  }
  for (int i = 1; i < n; i++) {
    node->next = (small_node *)((char *)node + sz);
  }
  node->next = pp->next;
  pp->next = node;
}

void *small_alloc::allocate(size_t sz) {
  void *result = nullptr;
  size_t tsz = up_to_two_power(sz);  // 上调至实际申请的 size
  uint16_t idx = get_index(tsz);
  DEBUG_DTL(_log, "sz up to %d, tsz up to %d, will allocate on free[%d]", sz,
            tsz, idx);
  if (nullptr != free[idx].next) {
    DEBUG_DTL(_log, "Try allocate on free[%d]", idx);
    result = free[idx].next;
    free[idx].next = ((small_node *)result)->next;
  } else {
    uint16_t n = NUMS;
    DEBUG_DTL(_log, "Try allocate on other free chrunk\n");
    result = malloc_chrunk(tsz, n);
    if (nullptr != result) {
      add_to_free(result, tsz, n);
      DEBUG_DTL(_log, "Allocate on free chrunk\n");
    }
  }
  if (nullptr == result) {
    DEBUG_DTL(_log, "Try allocate on system heap\n");
    result = smalloc(tsz);
  }
  if (nullptr != result) {
    DEBUG_DTL(_log, "Try adjust to fit\n");
    adjust_to_fit(((char *)result + tsz), idx, tsz - sz);
  }
  DEBUG_DTL(_log, "Get a new address on %p", result);
  return result;
}

void *small_alloc::smalloc(size_t tsz) {
  DEBUG_DTL(_log, "Will get a chrunk from system heap\n");
  /* 每次申请 MUNS 个 tsz 空闲块，再加上一些动态增加的 BIGSZ 空闲块作为内存池
   */
  void *result = malloc((tsz << small_node_nums(NUMS)) +
                        (heap_size >> small_node_nums(BIGSZ)));
  if (nullptr != result) {
    DEBUG_DTL(
        _log, "Got a chrunk from system heap on %p and size=%d\n", result,
        (tsz << small_node_nums(NUMS)) + (heap_size >> small_node_nums(BIGSZ)));
    /* 将 NUMS-1 个 tsz 加入对应的空闲块 */
    add_to_free(result, tsz, NUMS);
    DEBUG_DTL(_log, "Added the remain chrunk in free[%d]", get_index(tsz));

    /* 将多申请的加入到 BIGSZ 空闲块中，这里 - BIGSZ 是因为 add_to_free
     * 的实现是从第 1 个 sz
     * 大小开始添加的，而作为内存池的空闲块不需要减去开始的块
     */
    add_to_free(
        (void *)(((char *)result) + (tsz << small_node_nums(NUMS)) - BIGSZ),
        BIGSZ, (heap_size >> small_node_nums(BIGSZ)) + 1);
    heap_size += (tsz * NUMS) + (heap_size >> small_node_nums(BIGSZ));
  } else {
    result = oom_malloc(tsz);
  }
  return result;
}

void *small_alloc::malloc_chrunk(size_t sz, uint16_t &n) {
  void *result = nullptr;
  uint16_t min_idx = get_index(sz);
  uint16_t max_idx = get_index(sz * n);
  if (max_idx > small_node_nums(BIGSZ / MINSZ)) {
    max_idx = small_node_nums(BIGSZ / MINSZ);
  }
  /* 从中间往下找 */
  for (uint16_t i = max_idx; i > min_idx; i--) {
    if (nullptr == free[i].next) continue;
    result = free[i].next;
    n = (1 << (i - min_idx));
    free[i].next = ((small_node *)result)->next;
    break;
  }
  /* 中间未找到，向上找 */
  if (nullptr == result) {
    for (uint16_t i = max_idx; i < small_node_nums(BIGSZ / MINSZ); i++) {
      if (nullptr == free[i].next) continue;
      result = free[i].next;
      n = (1 << (i - min_idx));
      free[i].next = ((small_node *)result)->next;
      break;
    }
  }
  return result;
}

void small_alloc::adjust_to_fit(void *p, uint16_t idx, size_t sz) {
  if (sz < MINSZ) return;
  char *pc = (char *)p;
  uint16_t remain = sz;
  for (uint16_t i = idx - 1; remain >= MINSZ && i >= 0; i++) {
    if ((remain = sz & (1 << i)) < 0) continue;
    DEBUG_DTL(_log, "adjust_to_fit on %d\n", i);
    add_to_free((small_node *)pc, i);
    pc += (1 << i);
    sz = sz & (remain - 1);
  }
}
void *small_alloc::oom_malloc(size_t sz) { return nullptr; }
}  // namespace details

void *jmp_allocator::allocate(std::size_t sz) {
  sz = up_to_minsz_times(sz);
  if (sz < BIGSZ) return smem->allocate(sz);
  return bmem->allocate(sz);
}

void jmp_allocator::deallocate(void *ptr, std::size_t sz) {
  sz = up_to_minsz_times(sz);
  if (sz < BIGSZ)
    smem->deallocate(ptr, sz);
  else
    bmem->deallocate(ptr, sz);
}
}  // namespace jmp
