#import "ppl.h"
#import "xpf.h"
#include <choma/arm64.h>
#include <choma/PatchFinder.h>

uint64_t xpf_find_ppl_dispatch_section(void)
{
	uint32_t bAny = 0, bAnyMask = 0, movX15Any = 0, movX15AnyMask = 0;
	arm64_gen_b_l(OPT_BOOL(false), OPT_UINT64_NONE, OPT_UINT64_NONE, &bAny, &bAnyMask);
	arm64_gen_mov_imm('z', ARM64_REG_X(15), OPT_UINT64_NONE, OPT_UINT64(0), &movX15Any, &movX15AnyMask);

	uint32_t pplCallerInst[] = {
		movX15Any,
		bAny,
		movX15Any,
		bAny,
	};
	uint32_t pplCallerMask[] = {
		movX15AnyMask,
		bAnyMask,
		movX15AnyMask,
		bAnyMask,
	};

	__block uint64_t ppl_dispatch_section = 0;
	PFPatternMetric *metric = pfmetric_pattern_init(pplCallerInst,pplCallerMask,sizeof(pplCallerInst), sizeof(uint32_t));
	pfmetric_run(gXPF.kernelTextSection, metric, ^(uint64_t vmaddr, bool *stop) {
		ppl_dispatch_section = vmaddr;
		*stop = true;
	});
	pfmetric_free(metric);
	return ppl_dispatch_section;
}

uint64_t xpf_find_ppl_enter(void)
{
	uint64_t ppl_dispatch_section = xpf_item_resolve("kernelSymbol.ppl_dispatch_section");
	__block uint64_t ppl_enter = 0;
	arm64_dec_b_l(pfsec_read32(gXPF.kernelTextSection, ppl_dispatch_section+4), ppl_dispatch_section+4, &ppl_enter, NULL);
	return ppl_enter;
}

uint64_t xpf_find_ppl_bootstrap_dispatch(void)
{
	uint64_t ppl_enter = xpf_item_resolve("kernelSymbol.ppl_enter");

	uint32_t cbzAny = 0, cbzAnyMask = 0;
	arm64_gen_cb_n_z(OPT_BOOL(false), ARM64_REG_ANY, OPT_UINT64_NONE, &cbzAny, &cbzAnyMask);

	uint64_t cbzPPLDispatchAddr = pfsec_find_next_inst(gXPF.kernelTextSection, ppl_enter, 30, cbzAny, cbzAnyMask);
	if (cbzPPLDispatchAddr) {
		uint64_t ppl_bootstrap_dispatch = 0;
		arm64_dec_cb_n_z(pfsec_read32(gXPF.kernelTextSection, cbzPPLDispatchAddr), cbzPPLDispatchAddr, NULL, NULL, &ppl_bootstrap_dispatch);
		return ppl_bootstrap_dispatch;
	}
	else {
		uint32_t bCondAnyInst = 0, bCondAnyMask = 0;
		arm64_gen_b_c_cond(OPT_BOOL(false), OPT_UINT64_NONE, OPT_UINT64_NONE, ARM64_COND_ANY, &bCondAnyInst, &bCondAnyMask);
		uint64_t bcondPPLDispatchAddr = pfsec_find_next_inst(gXPF.kernelTextSection, ppl_enter, 30, bCondAnyInst, bCondAnyMask);
		if (bcondPPLDispatchAddr) {
			uint64_t ppl_bootstrap_dispatch = 0;
			arm64_dec_b_c_cond(pfsec_read32(gXPF.kernelTextSection, bcondPPLDispatchAddr), bcondPPLDispatchAddr, &ppl_bootstrap_dispatch, NULL, NULL);
			return ppl_bootstrap_dispatch;
		}
	}
	return 0;
}

uint64_t xpf_find_ppl_handler_table(void)
{
	uint64_t ppl_bootstrap_dispatch = xpf_item_resolve("kernelSymbol.ppl_bootstrap_dispatch");

	uint32_t addAny = 0, addAnyMask = 0;
	arm64_gen_add_imm(ARM64_REG_ANY, ARM64_REG_ANY, OPT_UINT64_NONE, &addAny, &addAnyMask);

	uint64_t addAddr = pfsec_find_next_inst(gXPF.kernelTextSection, ppl_bootstrap_dispatch, 30, addAny, addAnyMask);
	return pfsec_arm64_resolve_adrp_ldr_str_add_reference_auto(gXPF.kernelTextSection, addAddr);
}

uint64_t xpf_find_ppl_routine(uint32_t idx)
{
	uint64_t ppl_handler_table = xpf_item_resolve("kernelSymbol.ppl_handler_table");
	return xpfsec_read_ptr(gXPF.kernelDataConstSection, ppl_handler_table + (sizeof(uint64_t) * idx));
}

uint64_t xpf_find_ppl_dispatch_func(uint32_t idx)
{
	uint64_t ppl_dispatch_section = xpf_item_resolve("kernelSymbol.ppl_dispatch_section");

	uint32_t movToFind = 0, movMaskToFind = 0;
	arm64_gen_mov_imm('z', ARM64_REG_X(15), OPT_UINT64(idx), OPT_UINT64(0), &movToFind, &movMaskToFind);

	return pfsec_find_next_inst(gXPF.kernelTextSection, ppl_dispatch_section, 1000, movToFind, movMaskToFind);
}

uint64_t xpf_find_pmap_image4_trust_caches(void)
{
	uint64_t pmap_lookup_in_loaded_trust_caches_internal = xpf_item_resolve("kernelSymbol.pmap_lookup_in_loaded_trust_caches_internal");

	uint32_t ldrLitAny = 0, ldrLitAnyMask = 0;
	arm64_gen_ldr_lit(ARM64_REG_ANY, OPT_UINT64_NONE, &ldrLitAny, &ldrLitAnyMask);
	uint64_t ldrLitAddr = pfsec_find_next_inst(gXPF.kernelPPLTextSection, pmap_lookup_in_loaded_trust_caches_internal, 20, ldrLitAny, ldrLitAnyMask);
	if (ldrLitAddr) {
		int64_t ldrTarget = 0;
		arm64_dec_ldr_lit(pfsec_read32(gXPF.kernelPPLTextSection, ldrLitAddr), NULL, &ldrTarget);
		return ldrLitAddr + ldrTarget;
	}

	/*uint32_t ldrAny = 0, ldrAnyMask = 0;
	arm64_gen_ldr_imm(0, ARM64_REG_ANY, ARM64_REG_ANY, OPT_UINT64_NONE, &ldrAny, &ldrAnyMask);
	uint64_t ldrAddr = pfsec_find_next_inst(gXPF.kernelPPLTextSection, pmap_lookup_in_loaded_trust_caches_internal, 20, ldrAny, ldrAnyMask);
	if (ldrAddr) {
		uint32_t ldrInst = pfsec_read32(gXPF.kernelPPLTextSection, ldrAddr);
		arm64_register reg;
		uint64_t ldrImm = 0;
		arm64_dec_ldr_imm(ldrInst, NULL, &reg, &ldrImm, NULL);
		uint32_t adrpInst = 0, adrpInstAny = 0;
		arm64_gen_adr_p(OPT_BOOL(true), OPT_UINT64_NONE, OPT_UINT64_NONE, reg, &adrpInst, &adrpInstAny);
		uint64_t adrpAddr = pfsec_find_prev_inst(gXPF.kernelPPLTextSection, ldrAddr, 20, adrpInst, adrpInstAny);
		if (adrpAddr) {
			uint32_t adrpInst = pfsec_read32(gXPF.kernelPPLTextSection, adrpAddr);
			uint64_t adrpTarget = 0;
			arm64_dec_adr_p(adrpInst, adrpAddr, &adrpTarget, NULL, NULL);
			return adrpTarget + ldrImm;
		}
	}*/

	return 0;
}

uint64_t xpf_find_pmap_query_trust_cache_safe(void)
{
	uint64_t pmap_lookup_in_loaded_trust_caches_internal = xpf_item_resolve("kernelSymbol.pmap_lookup_in_loaded_trust_caches_internal");

	uint32_t blAny = 0, blAnyMask = 0;
	arm64_gen_b_l(OPT_BOOL(true), OPT_UINT64_NONE, OPT_UINT64_NONE, &blAny, &blAnyMask);

	uint64_t blAddr = pfsec_find_next_inst(gXPF.kernelPPLTextSection, pmap_lookup_in_loaded_trust_caches_internal, 30, blAny, blAnyMask);
	uint32_t blInst = pfsec_read32(gXPF.kernelPPLTextSection, blAddr);

	uint64_t pmap_query_trust_cache_safe = 0;
	arm64_dec_b_l(blInst, blAddr, &pmap_query_trust_cache_safe, NULL);
	return pmap_query_trust_cache_safe;
}

uint64_t xpf_find_ppl_trust_cache_rt(void)
{
	uint64_t pmap_query_trust_cache_safe = xpf_item_resolve("kernelSymbol.pmap_query_trust_cache_safe");

	uint32_t pacMovInst = 0, pacMovMask = 0;
	arm64_gen_mov_imm('z', ARM64_REG_ANY, OPT_UINT64(0x6653), OPT_UINT64(0), &pacMovInst, &pacMovMask);
	uint64_t pacMovAddr = pfsec_find_next_inst(gXPF.kernelPPLTextSection, pmap_query_trust_cache_safe, 50, pacMovInst, pacMovMask);

	uint32_t addX0AnyInst = 0, addX0AnyMask = 0;
	arm64_gen_add_imm(ARM64_REG_X(0), ARM64_REG_ANY, OPT_UINT64_NONE, &addX0AnyInst, &addX0AnyMask);
	uint64_t ppl_trust_cache_rt_refAddr = pfsec_find_prev_inst(gXPF.kernelPPLTextSection, pacMovAddr, 10, addX0AnyInst, addX0AnyMask);

	return pfsec_arm64_resolve_adrp_ldr_str_add_reference_auto(gXPF.kernelPPLTextSection, ppl_trust_cache_rt_refAddr);
}

uint64_t xpf_find_pmap_pin_kernel_pages(void)
{
	PFStringMetric *stringMetric = pfmetric_string_init("pmap_pin_kernel_pages");
	__block uint64_t pmap_pin_kernel_pages_stringAddr = 0;
	pfmetric_run(gXPF.kernelStringSection, stringMetric, ^(uint64_t vmaddr, bool *stop){
		pmap_pin_kernel_pages_stringAddr = vmaddr;
		*stop = true;
	});
	pfmetric_free(stringMetric);

	PFXrefMetric *xrefMetric = pfmetric_xref_init(pmap_pin_kernel_pages_stringAddr, XREF_TYPE_MASK_REFERENCE);
	__block uint64_t pmap_pin_kernel_pages = 0;
	pfmetric_run(gXPF.kernelTextSection, xrefMetric, ^(uint64_t vmaddr, bool *stop) {
		pmap_pin_kernel_pages = pfsec_find_function_start(gXPF.kernelTextSection, vmaddr);
		*stop = true;
	});
	pfmetric_free(xrefMetric);

	return pmap_pin_kernel_pages;
}

uint64_t xpf_find_pmap_pin_kernel_pages_reference(uint32_t idx)
{
	uint64_t pmap_pin_kernel_pages = xpf_item_resolve("kernelSymbol.pmap_pin_kernel_pages");

	uint32_t ldrAnyInst = 0, ldrAnyMask = 0;
	arm64_gen_ldr_imm(0, ARM64_REG_ANY, ARM64_REG_ANY, OPT_UINT64_NONE, &ldrAnyInst, &ldrAnyMask);

	__block uint64_t ref = 0;
	__block uint32_t f = 0;
	PFPatternMetric *metric = pfmetric_pattern_init(&ldrAnyInst, &ldrAnyMask, sizeof(ldrAnyInst), sizeof(uint32_t));
	pfmetric_run_in_range(gXPF.kernelTextSection, pmap_pin_kernel_pages, -1, metric, ^(uint64_t vmaddr, bool *stop) {
		arm64_register destinationReg;
		arm64_dec_ldr_imm(pfsec_read32(gXPF.kernelTextSection, vmaddr), &destinationReg, NULL, NULL, NULL);
		// On some kernels there is one additional ldr before the ones we're looking for
		// As this always loads into x0 and the other ones don't, we can filter it out based on that metric
		if (ARM64_REG_GET_NUM(destinationReg) != 0) {
			if (f == idx) {
				ref = pfsec_arm64_resolve_adrp_ldr_str_add_reference_auto(gXPF.kernelTextSection, vmaddr);
				*stop = true;
			}
			f++;
		}
	});

	return ref;
}

uint64_t xpf_find_tte_get_ptd(void)
{
	PFStringMetric *stringMetric = pfmetric_string_init("tte_get_ptd");
	__block uint64_t ptte_get_ptd_stringAddr = 0;
	pfmetric_run(gXPF.kernelStringSection, stringMetric, ^(uint64_t vmaddr, bool *stop){
		ptte_get_ptd_stringAddr = vmaddr;
		*stop = true;
	});
	pfmetric_free(stringMetric);

	uint32_t ret = gXPF.kernelIsArm64e ? 0xd65f0fff : 0xd65f03c0;

	PFXrefMetric *xrefMetric = pfmetric_xref_init(ptte_get_ptd_stringAddr, XREF_TYPE_MASK_REFERENCE);
	__block uint64_t tte_get_ptd = 0;
	pfmetric_run(gXPF.kernelTextSection, xrefMetric, ^(uint64_t vmaddr, bool *stop) {
		if (pfsec_find_prev_inst(gXPF.kernelTextSection, vmaddr, 10, ret, 0xffffffff)) {
			tte_get_ptd = pfsec_find_function_start(gXPF.kernelTextSection, vmaddr);
			*stop = true;
		}
	});
	pfmetric_free(xrefMetric);

	return tte_get_ptd;
}

uint64_t xpf_find_pv_head_table(void)
{
	uint64_t tte_get_ptd = xpf_item_resolve("kernelSymbol.tte_get_ptd");

	uint32_t ldrAnyInst = 0, ldrAnyMask = 0;
	arm64_gen_ldr_imm(0, ARM64_REG_ANY, ARM64_REG_ANY, OPT_UINT64_NONE, &ldrAnyInst, &ldrAnyMask);

	uint64_t ref1 = pfsec_find_next_inst(gXPF.kernelTextSection, tte_get_ptd, 0, ldrAnyInst, ldrAnyMask);
	uint64_t ref2 = pfsec_find_next_inst(gXPF.kernelTextSection, ref1+4, 0, ldrAnyInst, ldrAnyMask);

	return pfsec_arm64_resolve_adrp_ldr_str_add_reference_auto(gXPF.kernelTextSection, ref2);
}

void xpf_ppl_init(void)
{
	if (gXPF.kernelIsArm64e) {
		xpf_item_register("kernelSymbol.ppl_enter", xpf_find_ppl_enter, NULL);
		xpf_item_register("kernelSymbol.ppl_bootstrap_dispatch", xpf_find_ppl_bootstrap_dispatch, NULL);
		xpf_item_register("kernelSymbol.ppl_dispatch_section", xpf_find_ppl_dispatch_section, NULL);
		xpf_item_register("kernelSymbol.ppl_handler_table", xpf_find_ppl_handler_table, NULL);
		xpf_item_register("kernelSymbol.pmap_enter_options_internal", xpf_find_ppl_routine, (void *)(uint32_t)10);
		xpf_item_register("kernelSymbol.pmap_enter_options_ppl", xpf_find_ppl_dispatch_func, (void *)(uint32_t)10);
		xpf_item_register("kernelSymbol.pmap_lookup_in_loaded_trust_caches_internal", xpf_find_ppl_routine, (void *)(uint32_t)41);
		xpf_item_register("kernelSymbol.pmap_pin_kernel_pages", xpf_find_pmap_pin_kernel_pages, NULL);
		xpf_item_register("kernelSymbol.tte_get_ptd", xpf_find_tte_get_ptd, NULL);

		xpf_item_register("kernelSymbol.vm_first_phys", xpf_find_pmap_pin_kernel_pages_reference, (void *)(uint32_t)0);
		xpf_item_register("kernelSymbol.vm_last_phys", xpf_find_pmap_pin_kernel_pages_reference, (void *)(uint32_t)1);
		xpf_item_register("kernelSymbol.pp_attr_table", xpf_find_pmap_pin_kernel_pages_reference, (void *)(uint32_t)2);
		xpf_item_register("kernelSymbol.pv_head_table", xpf_find_pv_head_table, NULL);

		if (strcmp(gXPF.darwinVersion, "22.0.0") >= 0) {
			// iOS >=16
			xpf_item_register("kernelSymbol.pmap_query_trust_cache_safe", xpf_find_pmap_query_trust_cache_safe, NULL);
			xpf_item_register("kernelSymbol.ppl_trust_cache_rt", xpf_find_ppl_trust_cache_rt, NULL);
		}
		else {
			// iOS <=15
			xpf_item_register("kernelSymbol.pmap_image4_trust_caches", xpf_find_pmap_image4_trust_caches, NULL);
		}
	}
}