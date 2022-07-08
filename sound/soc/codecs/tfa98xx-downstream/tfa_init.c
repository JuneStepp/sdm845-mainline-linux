#include "tfa_service.h"
#include "tfa_internal.h"
#include "tfa_container.h"
#include "tfa98xx_tfafieldnames.h"

 /* The CurrentSense4 registers are not in the datasheet */
#define TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF (1<<2)
#define TFA98XX_CURRENTSENSE4 0x49

/***********************************************************************************/
/* GLOBAL (Defaults)                                                               */
/***********************************************************************************/
static enum Tfa98xx_Error no_overload_function_available(struct tfa_device *tfa, int not_used)
{
	(void)tfa;
	(void)not_used;

	return Tfa98xx_Error_Ok;
}

static enum Tfa98xx_Error no_overload_function_available2(struct tfa_device *tfa)
{
	(void)tfa;

	return Tfa98xx_Error_Ok;
}

/* tfa98xx_dsp_system_stable
*  return: *ready = 1 when clocks are stable to allow DSP subsystem access
*/
static enum Tfa98xx_Error tfa_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short status;
	int value;

	/* check the contents of the STATUS register */
	value = TFA_READ_REG(tfa, AREFS);
	if (value < 0) {
		error = -value;
		*ready = 0;
		WARN_ON(error);		/* an error here can be fatal */
		return error;
	}
	status = (unsigned short)value;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(tfa, AREFS, status) == 0)
		|| (TFA_GET_BF_VALUE(tfa, CLKS, status) == 0));

	return error;
}

/* tfa98xx_toggle_mtp_clock
 * Allows to stop clock for MTP/FAim needed for PLMA5505 */
static enum Tfa98xx_Error tfa_faim_protect(struct tfa_device *tfa, int state)
{
	(void)tfa;
	(void)state;

	return Tfa98xx_Error_Ok;
}

/** Set internal oscillator into power down mode.
 *
 *  This function is a worker for tfa98xx_set_osc_powerdown().
 *
 *  @param[in] tfa device description structure
 *  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
 *
 *  @return Tfa98xx_Error_Ok when successfull, error otherwise.
 */
static enum Tfa98xx_Error tfa_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return Tfa98xx_Error_Ok;
}
static enum Tfa98xx_Error tfa_update_lpm(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return Tfa98xx_Error_Ok;
}
static enum Tfa98xx_Error tfa_dsp_reset(struct tfa_device *tfa, int state)
{
	/* generic function */
	TFA_SET_BF_VOLATILE(tfa, RST, (uint16_t)state);

	return Tfa98xx_Error_Ok;
}

int tfa_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int mtpk, active_value = tfa->profile;

	/* Also set the new value in the struct */
	tfa->profile = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWPROFIL, new_value); /* set current profile */
	TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swprofile(struct tfa_device *tfa)
{
	return /*TFA_GET_BF(tfa, SWPROFIL) - 1*/tfa->profile;
}

static int tfa_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	int mtpk, active_value = tfa->vstep;

	/* Also set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWVSTEP, new_value); /* set current vstep */
	TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swvstep(struct tfa_device *tfa)
{
	int value = 0;
	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, SWVSTEP);

	/* Also set the new value in the struct */
	tfa->vstep = value - 1;

	return value - 1; /* invalid if 0 */
}

static int tfa_get_mtpb(struct tfa_device *tfa) {

	int value = 0;

	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, MTPB);

	return value;
}

static enum Tfa98xx_Error
tfa_set_mute_nodsp(struct tfa_device *tfa, int mute)
{
	(void)tfa;
	(void)mute;

	return Tfa98xx_Error_Ok;
}

static int tfa_set_bitfield(struct tfa_device* tfa, uint16_t bitfield, uint16_t value)
{
	return tfa_set_bf(tfa, (uint16_t)bitfield, value);
}

void tfa_set_ops_defaults(struct tfa_device_ops *ops)
{
	/* defaults */
	ops->tfa_reg_read = tfa98xx_read_register16;
	ops->tfa_reg_write = tfa98xx_write_register16;
	ops->tfa_mem_read = tfa98xx_dsp_read_mem;
	ops->tfa_mem_write = tfa98xx_dsp_write_mem_word;
	ops->tfa_dsp_msg = tfa_dsp_msg_rpc;
	ops->tfa_dsp_msg_read = tfa_dsp_msg_read_rpc;
	ops->dsp_write_tables = no_overload_function_available;
	ops->dsp_reset = tfa_dsp_reset;
	ops->dsp_system_stable = tfa_dsp_system_stable;
	ops->auto_copy_mtp_to_iic = no_overload_function_available2;
	ops->factory_trimmer = no_overload_function_available2;
	ops->phase_shift = no_overload_function_available2;
	ops->set_swprof = tfa_set_swprofile;
	ops->get_swprof = tfa_get_swprofile;
	ops->set_swvstep = tfa_set_swvstep;
	ops->get_swvstep = tfa_get_swvstep;
	ops->get_mtpb = tfa_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
	ops->faim_protect = tfa_faim_protect;
	ops->set_osc_powerdown = tfa_set_osc_powerdown;
	ops->update_lpm = tfa_update_lpm;
	ops->tfa_set_bitfield = tfa_set_bitfield;
}

/***********************************************************************************/
/* no TFA
 *  external DSP SB instance                                                               */
 /***********************************************************************************/
static short tfanone_swvstep, swprof; //TODO emulate in hal plugin
static enum Tfa98xx_Error tfanone_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	(void)tfa; /* suppress warning */
	*ready = 1; /* assume always ready */

	return Tfa98xx_Error_Ok;
}

static int tfanone_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	swprof = new_value;

	return active_value;
}

static int tfanone_get_swprofile(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */
	return swprof;
}

static int tfanone_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfanone_swvstep = new_value;

	return new_value;
}

static int tfanone_get_swvstep(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */
	return tfanone_swvstep;
}

void tfanone_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	tfa_set_ops_defaults(ops);

	ops->dsp_system_stable = tfanone_dsp_system_stable;
	ops->set_swprof = tfanone_set_swprofile;
	ops->get_swprof = tfanone_get_swprofile;
	ops->set_swvstep = tfanone_set_swvstep;
	ops->get_swvstep = tfanone_get_swvstep;

}

/***********************************************************************************/
/* TFA9894                                                                         */
/***********************************************************************************/
static int tfa9894_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;
	tfa_set_bf_volatile(tfa, TFA9894_BF_SWPROFIL, new_value);
	return active_value;
}

static int tfa9894_get_swprofile(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9894_BF_SWPROFIL) - 1;
}

static int tfa9894_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;
	tfa_set_bf_volatile(tfa, TFA9894_BF_SWVSTEP, new_value);
	return new_value;
}

static int tfa9894_get_swvstep(struct tfa_device *tfa)
{
	return tfa_get_bf(tfa, TFA9894_BF_SWVSTEP) - 1;
}

static int tfa9894_get_mtpb(struct tfa_device *tfa)
{
	int value = 0;
	value = tfa_get_bf(tfa, TFA9894_BF_MTPB);
	return value;
}

/** Set internal oscillator into power down mode for TFA9894.
*
*  This function is a worker for tfa98xx_set_osc_powerdown().
*
*  @param[in] tfa device description structure
*  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
*
*  @return Tfa98xx_Error_Ok when successfull, error otherwise.
*/
static enum Tfa98xx_Error tfa9894_set_osc_powerdown(struct tfa_device *tfa, int state)
{
	if (state == 1 || state == 0) {
		return -tfa_set_bf(tfa, TFA9894_BF_MANAOOSC, (uint16_t)state);
	}

	return Tfa98xx_Error_Bad_Parameter;
}

static enum Tfa98xx_Error tfa9894_faim_protect(struct tfa_device *tfa, int status)
{
	enum Tfa98xx_Error ret = Tfa98xx_Error_Ok;
	/* 0b = FAIM protection enabled 1b = FAIM protection disabled*/
	ret = tfa_set_bf_volatile(tfa, TFA9894_BF_OPENMTP, (uint16_t)(status));
	return ret;
}

static enum Tfa98xx_Error tfa9894_specific(struct tfa_device *tfa)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	unsigned short value, xor;

	if (tfa->in_use == 0)
		return Tfa98xx_Error_NotOpen;
	/* Unlock keys to write settings */
	error = tfa_reg_write(tfa, 0x0F, 0x5A6B);
	error = tfa_reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = tfa_reg_write(tfa, 0xA0, xor);
	pr_debug("Device REFID:%x\n", tfa->rev);
	/* The optimal settings */
	if (tfa->rev == 0x0a94) {
		/* V36 */
		/* ----- generated code start ----- */
		tfa_reg_write(tfa, 0x00, 0xa245); //POR=0x8245
		tfa_reg_write(tfa, 0x02, 0x51e8); //POR=0x55c8
		tfa_reg_write(tfa, 0x52, 0xbe17); //POR=0xb617
		tfa_reg_write(tfa, 0x57, 0x0344); //POR=0x0366
		tfa_reg_write(tfa, 0x61, 0x0033); //POR=0x0073
		tfa_reg_write(tfa, 0x71, 0x00cf); //POR=0x018d
		tfa_reg_write(tfa, 0x72, 0x34a9); //POR=0x44e8
		tfa_reg_write(tfa, 0x73, 0x3808); //POR=0x3806
		tfa_reg_write(tfa, 0x76, 0x0067); //POR=0x0065
		tfa_reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		tfa_reg_write(tfa, 0x81, 0x5715); //POR=0x561a
		tfa_reg_write(tfa, 0x82, 0x0104); //POR=0x0044
		/* ----- generated code end   ----- */
	}
	else if (tfa->rev == 0x1a94) {
		/* V17 */
		/* ----- generated code start ----- */
		tfa_reg_write(tfa, 0x00, 0xa245); //POR=0x8245
		tfa_reg_write(tfa, 0x01, 0x15da); //POR=0x11ca
		tfa_reg_write(tfa, 0x02, 0x5288); //POR=0x55c8
		tfa_reg_write(tfa, 0x52, 0xbe17); //POR=0xb617
		tfa_reg_write(tfa, 0x53, 0x0dbe); //POR=0x0d9e
		tfa_reg_write(tfa, 0x56, 0x05c3); //POR=0x07c3
		tfa_reg_write(tfa, 0x57, 0x0344); //POR=0x0366
		tfa_reg_write(tfa, 0x61, 0x0032); //POR=0x0073
		tfa_reg_write(tfa, 0x71, 0x00cf); //POR=0x018d
		tfa_reg_write(tfa, 0x72, 0x34a9); //POR=0x44e8
		tfa_reg_write(tfa, 0x73, 0x38c8); //POR=0x3806
		tfa_reg_write(tfa, 0x76, 0x0067); //POR=0x0065
		tfa_reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		tfa_reg_write(tfa, 0x81, 0x5799); //POR=0x561a
		tfa_reg_write(tfa, 0x82, 0x0104); //POR=0x0044
		/* ----- generated code end ----- */

	}
	else if (tfa->rev == 0x2a94 || tfa->rev == 0x3a94) {
		/* ----- generated code start ----- */
		/* -----  version 25.00 ----- */
		tfa_reg_write(tfa, 0x01, 0x15da); //POR=0x11ca
		tfa_reg_write(tfa, 0x02, 0x51e8); //POR=0x55c8
		tfa_reg_write(tfa, 0x04, 0x0200); //POR=0x0000
		tfa_reg_write(tfa, 0x52, 0xbe17); //POR=0xb617
		tfa_reg_write(tfa, 0x53, 0x0dbe); //POR=0x0d9e
		tfa_reg_write(tfa, 0x57, 0x0344); //POR=0x0366
		tfa_reg_write(tfa, 0x61, 0x0032); //POR=0x0073
		tfa_reg_write(tfa, 0x71, 0x6ecf); //POR=0x6f8d
		tfa_reg_write(tfa, 0x72, 0xb4a9); //POR=0x44e8
		tfa_reg_write(tfa, 0x73, 0x38c8); //POR=0x3806
		tfa_reg_write(tfa, 0x76, 0x0067); //POR=0x0065
		tfa_reg_write(tfa, 0x80, 0x0000); //POR=0x0003
		tfa_reg_write(tfa, 0x81, 0x5799); //POR=0x561a
		tfa_reg_write(tfa, 0x82, 0x0104); //POR=0x0044
	/* ----- generated code end   ----- */
	}
	return error;
}

static enum Tfa98xx_Error
tfa9894_set_mute(struct tfa_device *tfa, int mute)
{
	tfa_set_bf(tfa, TFA9894_BF_CFSM, (const uint16_t)mute);
	return Tfa98xx_Error_Ok;
}

static enum Tfa98xx_Error tfa9894_dsp_system_stable(struct tfa_device *tfa, int *ready)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	/* check CLKS: ready if set */
	*ready = tfa_get_bf(tfa, TFA9894_BF_CLKS) == 1;

	return error;
}

void tfa9894_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	tfa_set_ops_defaults(ops);

	ops->tfa_init = tfa9894_specific;
	ops->dsp_system_stable = tfa9894_dsp_system_stable;
	ops->set_mute = tfa9894_set_mute;
	ops->faim_protect = tfa9894_faim_protect;
	ops->get_mtpb = tfa9894_get_mtpb;
	ops->set_swprof = tfa9894_set_swprofile;
	ops->get_swprof = tfa9894_get_swprofile;
	ops->set_swvstep = tfa9894_set_swvstep;
	ops->get_swvstep = tfa9894_get_swvstep;
	//ops->auto_copy_mtp_to_iic = tfa9894_auto_copy_mtp_to_iic;
	ops->set_osc_powerdown = tfa9894_set_osc_powerdown;
}
