#ifndef __SLICE_ATTEST_H

int slice_attest(struct sbi_domain *slice_dom, const unsigned char *data,
                 int size);

bool slice_measure(struct sbi_domain * slice_dom, unsigned char *buf, int size);

#endif