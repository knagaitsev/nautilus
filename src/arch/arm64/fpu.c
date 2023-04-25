
#include<nautilus/fpu.h>

#include<arch/arm64/sys_reg.h>

void fpu_init(struct naut_info *, int is_ap) {

  feat_acc_ctrl_reg_t cpacr;
  load_feat_acc_ctrl_reg(&cpacr);

  cpacr.fp_enabled = 1;
  cpacr.sve_enabled = 1;

  store_feat_acc_ctrl_reg(&cpacr);

  fp_ctrl_reg_t fpcr;
  load_fp_ctrl_reg(&fpcr);

  // Here we can modify the fpu settings

  store_fp_ctrl_reg(&fpcr);
}

