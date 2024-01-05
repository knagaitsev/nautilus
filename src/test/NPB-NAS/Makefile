#LDFLAGS += --allow-multiple-definition
# KJH - Used to be O3
CFLAGS += -O2 #-w
#-fnoopenmp-use-tls
obj-y += math/
obj-y += common/\
	rand/
#EP and MG conflicts with others

obj-$(NAUT_CONFIG_NPB_NAS_CG) += CG/
obj-$(NAUT_CONFIG_NPB_NAS_BT) += BT/
obj-$(NAUT_CONFIG_NPB_NAS_LU) += LU/
obj-$(NAUT_CONFIG_NPB_NAS_FT) += FT/
obj-$(NAUT_CONFIG_NPB_NAS_SP) += SP/
obj-$(NAUT_CONFIG_NPB_NAS_MG) += MG/
obj-$(NAUT_CONFIG_NPB_NAS_EP) += EP/
obj-$(NAUT_CONFIG_NPB_NAS_IS) += IS/

#obj-y+= common/\
	rand/\
	FT/

#obj-y += common/\
	rand/\
	SP/

#obj-y += common/\
         rand/\
	 BT/

#obj-y += common/\
	rand/\
	EP/

#obj-y += common/\
	rand/\
	MG/

#obj-y += CG/\
	common/\
	rand/

#obj-y += LU/\
	common/\
	rand/

#IS
#obj-y += IS/

