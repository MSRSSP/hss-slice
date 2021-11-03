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
#include "slice/slice_mgr.h"
#include "slice/slice_pmp.h"

struct tinycli_key {
  const int tokenId;
  const char *const name;
  const char *const helpString;
};

static bool tinyCLI_NameToKeyIndex_(struct tinycli_key const *const keys,
                                    size_t numKeys, char const *const pToken,
                                    size_t *pIndex);
static void slice_sw_start(int dom_index);

static void slice_sw_start(int dom_index) {
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
      slice_send_ipi_to_domain(dom_index, SLICE_IPI_SW_STOP);
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
static void slice_create_cli(size_t narg, const char **argv) {
  if (narg < 6) {
    mHSS_FANCY_PRINTF(LOG_NORMAL, "Please use 6 arguments cpu_mask mem_start "
                                  "mem_size image_from image_size fdt_from\n");
    return;
  }
  unsigned long cpu_mask = strtoul(argv[0], 0, 16);
  unsigned long mem_start = strtoul(argv[1], 0, 16);
  unsigned long mem_size = strtoul(argv[2], 0, 16);
  unsigned long image_from = strtoul(argv[3], 0, 16);
  unsigned long image_size = strtoul(argv[4], 0, 16);
  unsigned long fdt_from = strtoul(argv[5], 0, 16);
  int err = slice_create(cpu_mask, mem_start, mem_size, image_from, image_size,
                         fdt_from);
  if (err) {
    mHSS_FANCY_PRINTF(LOG_NORMAL, "slice_create returns err %d\n", err);
    ;
  }
}

static void slice_help(const struct tinycli_key *debugKeys, size_t nKeys) {
  for (size_t i = 0; i < nKeys; ++i) {
    mHSS_FANCY_PRINTF(LOG_NORMAL, "slice %s -- %s\n", debugKeys[i].name,
                      debugKeys[i].helpString);
  }
}

void tinyCLI_Slice(size_t narg, const char **argv_tokenArray) {
  size_t keyIndex;
  enum slice_cmd {
    SLICE_STOP,
    SLICE_START,
    SLICE_CREATE,
    SLICE_DELETE,
    SLICE_DUMP,
    SLICE_PMP,
    SLICE_HELP,
    SLICE_END,
  };
  const struct tinycli_key debugKeys[] = {
      {SLICE_STOP, "STOP", "stop a slice."},
      {SLICE_START, "START", "start a slice."},
      {SLICE_CREATE, "CREATE", "create a slice."},
      {SLICE_DELETE, "DELETE", "delete a slice."},
      {SLICE_DUMP, "DUMP", "dump slice info."},
      {SLICE_PMP, "PMP", "dump pmp info."},
      {SLICE_HELP, "help", "slice help."},
  };
  int dom_index = -1;
  unsigned int base_arg_idx = 0;
  if (narg > 1) {
    dom_index = strtoul(argv_tokenArray[base_arg_idx + 1], 0, 10);
  }
  if (narg > 0 &&
      tinyCLI_NameToKeyIndex_(debugKeys, ARRAY_SIZE(debugKeys),
                              argv_tokenArray[base_arg_idx], &keyIndex)) {
    switch (keyIndex) {
    case SLICE_STOP: {
      if (dom_index > 0) {
        slice_send_ipi_to_domain(dom_index, SLICE_IPI_SW_STOP);
      }
      break;
    }
    case SLICE_START: {
      slice_sw_start(dom_index);
      break;
    }
    case SLICE_DELETE: {
      slice_delete(dom_index);
      break;
    }
    case SLICE_CREATE: {
      slice_create_cli(narg - 1,
                       (const char **)&argv_tokenArray[base_arg_idx + 1]);
      break;
    }
    case SLICE_DUMP: {
      sbi_domain_dump_all("");
      break;
    }
    case SLICE_PMP: {
      if (dom_index >= 0) {
        slice_send_ipi_to_domain(dom_index, SLICE_IPI_PMP_DEBUG);
      } else {
        slice_pmp_dump();
      }
      break;
    }
    default:
      slice_help(debugKeys, SLICE_END);
      break;
    }
  }
}
