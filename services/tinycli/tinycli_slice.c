#include <stdlib.h>
#include <string.h> // strcasecmp(), strtok(), strtok_r()
#include <strings.h>

#include "assert.h"
#include "config.h"
#include "hss_boot_service.h"
#include "hss_debug.h"
#include "hss_types.h"
#include "sbi/sbi_domain.h"
#include "sbi/sbi_hart.h"
#include "sbi/sbi_hartmask.h"
#include "slice/slice.h"
#include "slice/slice_mgr.h"
#include "slice/slice_pmp.h"
#include "tinycli_helper.h"

struct tinycli_key {
  const int tokenId;
  const char *const name;
  const char *const helpString;
};

static bool tinyCLI_NameToKeyIndex_(struct tinycli_key const *const keys,
                                    size_t numKeys, char const *const pToken,
                                    size_t *pIndex);
void slice_sw_start(int dom_index);

__attribute__((weak)) void slice_sw_start(int dom_index) {
  if (dom_index < 1) {
    mHSS_FANCY_PRINTF(LOG_NORMAL, "Please use 1 arguments dom_index > 0. "
                                  "Refer to the dom_index from slice dump.\n");
    return;
  }
  unsigned int index_out, hart_id;
  struct sbi_domain *dom = sbi_index_to_domain(dom_index);
  sbi_hartmask_for_each_hart(hart_id, &dom->assigned_harts) {
    if (!HSS_Boot_SBISetupRequest(hart_id, &index_out)) {
      mHSS_FANCY_PRINTF(LOG_NORMAL, "%s: cannot start: hart_id=%d\n", __func__,
                        hart_id);
      slice_stop(dom_index);
      break;
    }
  }
}

static bool tinyCLI_NameToKeyIndex_(struct tinycli_key const *const keys,
                                    size_t numKeys, char const *const pToken,
                                    size_t *pIndex) {
  bool result = false;
  size_t i;

  assert(keys);
  assert(pToken);
  assert(pIndex);

  for (i = 0u; i < numKeys; i++) { // check for full match
    if (strncasecmp(keys[i].name, pToken, strlen(keys[i].name)) == 0) {
      result = true;
      *pIndex = i;
      break;
    }
  }

  if (!result) { // if no match found, check for partial match
    size_t count = 0u;
    for (i = 0u; i < numKeys; i++) {
      if (strncasecmp(keys[i].name, pToken, strlen(pToken)) == 0) {
        *pIndex = i;
        count++;
      }
    }

    if (count == 1u) {
      result = true; // multiple matches => ambiguity
    }
  }

  return result;
}

// slice create 0x10 0x1000000000 0x20000000 0xa00026f0 0x16e7400 0xa16f7af0
// slice create 0x6 0x80000000 0x20000000 0xa00106f0 0x16e7400 0xa16f7af0
#define PRV_S 1
static void slice_create_cli_help(void) {
  mHSS_FANCY_PRINTF(LOG_NORMAL, "Please use options arguments "
                                "-c cpu_mask -m mem_start -s mem_size "
                                "-i image_from -z image_size -f fdt_from\n");
}
static void slice_create_cli(size_t narg, const char **argv) {
  struct slice_options options = {0};
  options.fdt_from = 0xa36e7af0;
  options.image_from = 0xa20006f0;
  options.image_size = 0x16e7400;
  options.guest_mode = PRV_S;
  int opt;
  optind = 1;
  while ((opt = getopt(narg, (char **)argv, "c:m:s:i:z:f:u:h")) != -1)
    switch (opt) {
    case 'c':
      options.hartmask.bits[0] = strtoul(optarg, NULL, 16);
      break;
    case 'm':
      options.mem_start = strtoul(optarg, 0, 16);
      break;
    case 's':
      options.mem_size = strtoul(optarg, 0, 16);
      break;
    case 'i':
      options.image_from = strtoul(optarg, 0, 16);
      break;
    case 'z':
      options.image_size = strtoul(optarg, 0, 16);
      break;
    case 'f':
      options.fdt_from = strtoul(optarg, 0, 16);
      break;
    case 'u':
      memcpy(&(options.stdout[0]), optarg, 32);
      break;
    default:
      slice_create_cli_help();
      break;
    }
  int err = slice_create_full(&options);
  if (err) {
    mHSS_FANCY_PRINTF(LOG_NORMAL, "slice_create returns err %d\n", err);
    slice_create_cli_help();
  }
}

static void slice_help(const struct tinycli_key *debugKeys, size_t nKeys) {
  for (size_t i = 0; i < nKeys; ++i) {
    mHSS_FANCY_PRINTF(LOG_NORMAL, "slice %s -- %s\n", debugKeys[i].name,
                      debugKeys[i].helpString);
  }
}

#ifndef TINY_TCB
static void slice_ipi_test_cli(int dom_index, size_t narg, const char **argv) {
  bool done = false;
  HSSTicks_t last_sec_time = HSS_GetTime();
  uint8_t cBuf[1];
  // default ticks = 100;
  unsigned long ticks = 500;
  int opt;
  // parse from the argv[2]
  optind = 2;
  while ((opt = getopt(narg, (char **)argv, "t:h")) != -1)
    switch (opt) {
    case 't':
      mHSS_FANCY_PRINTF(LOG_NORMAL, "slice ipi dom_index -t %s\n", optarg);
      ticks = strtoul(optarg, 0, 10);
      break;
    default:
      mHSS_FANCY_PRINTF(LOG_NORMAL, "slice ipi dom_index -t ticks");
      break;
    }
  mHSS_FANCY_PRINTF(LOG_NORMAL, "run ipi test per %d ticks, narg= %d\n", ticks,
                    narg);
  while (!done) {
    if (HSS_Timer_IsElapsed(last_sec_time, ticks)) {
      slice_ipi_test(dom_index);
      last_sec_time = HSS_GetTime();
    }
  }
}
#endif

extern void slice_process_cache_mask(int dom_index, uint64_t mask);

void tinyCLI_Slice(unsigned narg, const char **argv_tokenArray) {
  size_t keyIndex;
  enum slice_cmd {
    SLICE_STOP,
    SLICE_START,
    SLICE_CREATE,
    SLICE_DELETE,
#ifndef TINY_TCB
    SLICE_DUMP,
    SLICE_HW_RESET,
    SLICE_PMP,
    SLICE_IPI_TEST,
    SLICE_CACHE_MASK,
#endif
    SLICE_HELP,
    SLICE_END,
  };
  const struct tinycli_key debugKeys[] = {
      {SLICE_STOP, "STOP", "stop a slice."},
      {SLICE_START, "START", "start a slice."},
      {SLICE_CREATE, "CREATE", "create a slice."},
      {SLICE_DELETE, "DELETE", "delete a slice."},
#ifndef TINY_TCB
      {SLICE_DUMP, "DUMP", "dump slice info."},
      {SLICE_HW_RESET, "RESET",
       "reset a slice via per-core reset unit (Only work in QEMU)."},
      {SLICE_PMP, "PMP", "dump pmp info."},
      {SLICE_IPI_TEST, "IPI", "send continuous ipi to a domain."},
      {SLICE_CACHE_MASK, "CACHE", "set or read cache config for a domain."},
#endif
      {SLICE_HELP, "help", "slice help."},
  };
  int dom_index = -1;
  unsigned int base_arg_idx = 0;

  if (narg > 0 &&
      tinyCLI_NameToKeyIndex_(debugKeys, ARRAY_SIZE(debugKeys),
                              argv_tokenArray[base_arg_idx], &keyIndex)) {
    switch (keyIndex) {
    case SLICE_STOP:
    case SLICE_DELETE:
    case SLICE_START:
#ifndef TINY_TCB
    case SLICE_DUMP:
    case SLICE_HW_RESET:
    case SLICE_PMP:
    case SLICE_IPI_TEST:
    case SLICE_CACHE_MASK:
#endif
      if (narg > 1) {
        dom_index = strtoul(argv_tokenArray[base_arg_idx + 1], 0, 10);
      }
      break;
    default:
      break;
    }
    switch (keyIndex) {
    case SLICE_STOP: {
      if (dom_index > 0) {
        slice_stop(dom_index);
      }
      break;
    }
    case SLICE_START: {
      slice_sw_start(dom_index);
      break;
    }
    case SLICE_DELETE: {
      mHSS_FANCY_PRINTF(LOG_NORMAL, "%s: delete %d\n", __func__, dom_index);
      slice_delete(dom_index);
      break;
    }
    case SLICE_CREATE: {
      slice_create_cli(narg, (const char **)&argv_tokenArray[base_arg_idx]);
      break;
    }
#ifndef TINY_TCB
    case SLICE_HW_RESET: {
      break;
    }
    case SLICE_DUMP: {
      dump_slices_config();
      break;
    }
    case SLICE_PMP: {
      slice_pmp_dump_by_index(dom_index);
      break;
    }
    case SLICE_IPI_TEST: {
      slice_ipi_test_cli(dom_index, narg,
                         (const char **)&argv_tokenArray[base_arg_idx]);
      break;
    }
    case SLICE_CACHE_MASK: {
      uint64_t mask = strtoul(argv_tokenArray[base_arg_idx + 2], 0, 0);
      slice_process_cache_mask(dom_index, mask);
      break;
    }
#endif
    default:
      slice_help(debugKeys, SLICE_END);
      break;
    }
  }
}
