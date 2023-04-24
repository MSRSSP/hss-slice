#include "libsig.h"
#include <sbi/riscv_asm.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_string.h>
#include <slice/slice.h>
#include "slice_attest.h"

#define REPORT_DATA_SIZE 64
#define SHA_ALG_TYPE SHA384
#define SIG_ALG_TYPE ECDSA

typedef struct _slice_digest {
  uint8_t buf[48];
} slice_digest;

typedef struct _slice_signature {
  uint8_t buf[128];
} slice_signature;

typedef struct {
  u8 buf[REPORT_DATA_SIZE];
  u64 mem_size;
  u32 hart_counts;
  slice_digest digest; // SHA384
} slice_report;

typedef struct {
  slice_report report;
  slice_signature signature;
} slice_attestation;

void slice_print_bytes(const unsigned char *buf, int size) {
  for (int i = 0; i < size; ++i) {
    slice_info("%02x", buf[i]);
  }
  slice_info("\n");
}

static uint8_t u8siglen;
static ec_key_pair *keypair = NULL;
static ec_sig_alg_type sig_type;
static hash_alg_type hash_type;
static ec_params params;

static int string_to_params(const char *ec_name, const char *ec_sig_name,
                            ec_sig_alg_type *sig_type,
                            const ec_str_params **ec_str_p,
                            const char *hash_name, hash_alg_type *hash_type) {
  const ec_str_params *curve_params;
  const ec_sig_mapping *sm;
  const hash_mapping *hm;
  u32 curve_name_len;

  if (sig_type != NULL) {
    /* Get sig type from signature alg name */
    sm = get_sig_by_name(ec_sig_name);
    if (!sm) {
      slice_error("Error: signature type %s is unknown!\n", ec_sig_name);
      goto err;
    }
    *sig_type = sm->type;
  }

  if (ec_str_p != NULL) {
    /* Get curve params from curve name */
    curve_name_len = local_strlen((const char *)ec_name) + 1;
    if (curve_name_len > 255) {
      /* Sanity check */
      goto err;
    }
    curve_params =
        ec_get_curve_params_by_name((const u8 *)ec_name, (u8)curve_name_len);
    if (!curve_params) {
      slice_error("Error: EC curve %s is unknown!\n", ec_name);
      goto err;
    }
    *ec_str_p = curve_params;
  }

  if (hash_type != NULL) {
    /* Get hash type from hash alg name */
    hm = get_hash_by_name(hash_name);
    if (!hm) {
      slice_error("Error: hash function %s is unknown!\n", hash_name);
      goto err;
    }
    *hash_type = hm->type;
  }

  return 0;

err:
  return -1;
}

void slice_report_dump(const slice_attestation *attest) {
  slice_report* report = &attest->report;
  slice_info("signature:\n");
  slice_print_bytes(&attest->signature, u8siglen);
  slice_info("report dump\n");
  slice_print_bytes(&report->buf[9], sizeof(slice_report));
  slice_info("\thart_count = %d\n", report->hart_counts);
  slice_info("\tmem_size = %lx\n", report->mem_size);
  slice_info("\tdigest:\n\t");
  slice_print_bytes(&report->digest, sizeof(report->digest));
  slice_info("\tdata:\n\t");
  slice_print_bytes(report->buf, REPORT_DATA_SIZE);

}

int slice_key_init(void) {
#define X509_ASN1_DER_KEY_OFFSET (24)
#include "bypass-uboot/keys/key_private_key.h"

  const ec_str_params *ec_str_p;
  int ret = 0;
  if (keypair) {
    goto out;
  }
  keypair = (ec_key_pair *)slice0_alloc_private(sizeof(ec_key_pair));

  /************************************/
  /* Get parameters from pretty names */
  if (string_to_params("SECP384R1", "ECDSA", &sig_type, &ec_str_p, "SHA384",
                       &hash_type)) {
    goto out;
  }
  /* Import the parameters */
  import_params(&params, ec_str_p);
  ret = ec_get_sig_len(&params, sig_type, hash_type, (uint8_t *)&u8siglen);
  if (ret) {
    slice_error("%s: bad key params\n", __func__);
    goto out;
  }
  ret = ec_structured_key_pair_import_from_priv_key_buf(
      keypair, &params, (u8 *)SECP384R1_ECDSA_private_key,
      sizeof(SECP384R1_ECDSA_private_key), sig_type);
out:
  return ret;
}

int slice_attest(struct sbi_domain *slice_dom, const unsigned char *data,
                 int size) {
  int hartid;
  unsigned char ecdsaSig[96];
  int ret;
  slice_attestation attestation;
  slice_report *report = &attestation.report;
  struct ec_sign_context sig_ctx;
  if (!slice_dom) {
    return 0;
  }
  memset(report, 0, sizeof(report));
  sbi_memcpy(&report->digest, &slice_dom->slice_digest, sizeof(report->digest));
  report->mem_size = slice_dom->slice_mem_size;
  report->hart_counts = 0;
  sbi_hartmask_for_each_hart(hartid, &slice_dom->assigned_harts) {
    report->hart_counts++;
  }
  if(data)
    sbi_memcpy(&report->buf[0], data, size);
  slice_key_init();
  ret = ec_sign(&attestation.signature.buf[0], u8siglen, keypair, (u8 *)&report->buf[0],
                sizeof(slice_report), sig_type, hash_type, NULL, 0);
  if(ret!=0) {
    slice_info("failed EC sign %d.\n", ret);
  } else {
    slice_info("EC sign Ok! report size: %d, signature size: %d\n",sizeof(slice_report), u8siglen);
  }
  slice_verify(&attestation);
  return 0;
}

bool slice_verify(const unsigned char *raw_attestation) {
#include "bypass-uboot/keys/key_public_key.h"
  const slice_attestation *attestation =
      (const slice_attestation *)raw_attestation;
  ec_pub_key pub_key;
  int ret = ec_structured_pub_key_import_from_buf(
      &pub_key, &params, SECP384R1_ECDSA_public_key,
      sizeof(SECP384R1_ECDSA_public_key), sig_type);
  ret = ec_verify(&attestation->signature.buf[0], u8siglen, &pub_key,
                  (u8 *)&attestation->report.buf[0], sizeof(slice_report),
                  sig_type, hash_type, NULL, 0);
  slice_info("verification %s.\n", (ret == 0) ? "success" : "failed");
  slice_report_dump(attestation);
  return ret;
}

bool slice_measure(struct sbi_domain *slice_dom, unsigned char *buf, int size) {
  if (!slice_dom) {
    return;
  }
  slice_info("%s [hart %d] measure image_size=%x, src=%lx\n", __func__,
             current_hartid(), size, buf);
  const hash_mapping *hash_m = get_hash_by_name("SHA384");
  hash_context hctxt;
  slice_digest digest;
  size_t i;
  hash_m->hfunc_init(&hctxt);
  hash_m->hfunc_update(&hctxt, (unsigned char *)buf, size);
  hash_m->hfunc_finalize(&hctxt, &digest);
  slice_info("digest:\n");
  slice_print_bytes(&digest, sizeof(digest));
  /*
  hash_m->hfunc_init(&hctxt);
  for (i = 0; i < sizeof(digest); ++i) {
    digest.buf[i] |= slice_dom->slice_digest.buf[i];
  }
  
  hash_m->hfunc_update(&hctxt, &digest, sizeof(digest));
  hash_m->hfunc_finalize(&hctxt, &digest);
  */
  memcpy(&slice_dom->slice_digest, &digest, sizeof(digest));
  return true;
}
