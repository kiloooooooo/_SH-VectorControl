/***********************************************************************/
/*                                                                     */
/*  FILE        :VectorControl.c                                       */
/*  DATE        :Mon, Sep 23, 2019                                     */
/*  DESCRIPTION :Main Program                                          */
/*  CPU TYPE    :SH7125                                                */
/*                                                                     */
/*  This file is generated by Renesas Project Generator (Ver.4.19).    */
/*  NOTE:THIS IS A TYPICAL EXAMPLE.                                    */
/***********************************************************************/

//#include "typedefine.h"
#ifdef __cplusplus
//#include <ios>                        // Remove the comment when you use ios
//_SINT ios_base::Init::init_cnt;       // Remove the comment when you use ios
#endif

void main(void);
#ifdef __cplusplus
extern "C"
{
    void abort(void);
}
#endif

/*
 *                                       PIN
 *                 CN1                                           CN2
 *  PE00 ADSTATE| 01 02 |    PE01 ADSTATE |    PA00   F/B     | 01 02 |  PA01    BST
 *  PE02 NONE   | 03 04 |    PE03 FBRK    |    PA02   PBRK    | 01 02 |  PA03    RX
 *  PE04 DMS    | 05 06 |    PE05 NONE    |    PA04   TX      | 05 06 |  PA05    RES0
 *  PE06 PROTECT| 07 08 |    PE07 NONE    |    PA06   RES1    | 07 08 |  PA07    RES2
 *  PE08 PWMTGL | 09 10 |    PE09 PWM_U   |    PA08   RES3    | 09 10 |  PA09    RES4
 *  PE10 NC     | 11 12 |    PE11 PWM_U'  |    PA10   RES5    | 11 12 |  PA11    RES6
 *  PE12 PWM_V  | 13 14 |    PE13 PWM_W   |    PA12   RES7    | 13 14 |  PA13    RES8
 *  PE14 PWM_V' | 15 16 |    PE15 PWM_W'  |    PA14   RES9    | 15 16 |  PA15    NONE
 *  PF00 I_U    | 17 18 |    PF01 I_V     |    PB01   NONE    | 17 18 |  PB02    NONE
 *  PF02 I_W    | 19 20 |    PF03 I_DC    |    PB03   NONE    | 19 20 |  PB05    NONE
 *  PF04 ACC1   | 21 22 |    PF05 ACC2    |    PB16   NONE    | 21 22 |  RES     NONE
 *  PF06 NONE   | 23 24 |    PF07 NONE    |    NMI    NONE    | 23 24 |  WDTOVF  NONE
 *  MD1  NONE   | 25 26 |    FWE  NONE    |    ASEMD0 NONE    | 25 26 |  FWE     NONE
 *  Vcc  NONE   | 27 28 |    Vcc  NONE    |    GND    NONE    | 27 28 |  GND     NONE
 * 
 *  ---------------------------------------------------------------------------------
 *     FBRK = �t�b�g�u���[�L
 *     F/B  = �O��i�X�C�b�`
 *     BST  = �u�[�X�g
 *     PBRK = �p�[�L���O�u���[�L
 */

#include "iodefine.h"
#include "mathf.h"
#include "cpwm.h"

#define PI 3.141592653589793
#define PHI 2.094395102392667       // 2�� / 3
#define K_P 1.0f                    // P����Q�C��
#define K_I 1.0f                    // I����Q�C��
#define MAX_CURRENT 100.0f          // �d���Z���T��� [A]
#define CPWM_CARRIER_CYCLE 8000     // ����PWM�L�����A����
#define CPWM_DEADTIME 100           // ����PWM�f�b�h�^�C��
#define TGR_ADJUSTER 10             // TGR�␳�p
#define TGR_LIMIT 4000              // TGR����l
#define VOLTAGE_ADJUSTER_PERCENT 80 // �d���l�␳�p
#define VOLTAGE_MAX 100.0f          // �d���̍ő�l�A����ȏ�Ȃ炱�̒l�ɌŒ�
#define VOLTAGE_MIN -100.0f         // �d���̍ŏ��l�A����ȉ��Ȃ炱�̒l�ɌŒ�

typedef struct struct_tgr_values
{
    int u;
    int v;
    int w;
} tgr_values;

float integral_delta_i_d = 0.0f; // ��d(��I_d)
float integral_delta_i_q = 0.0f; // ��d(��I_q)
float previous_theta = 0.0f;     // �O��v���������[�^��]�p

int adjust_tgr(int tgr_value)
{
    // (TGR_LIMIT - (2 * (CPWM_DEADTIME + TGR_ADJUSTER))) / TGR_LIMIT
    float e = 1.0f - (2.0f * (float)(CPWM_DEADTIME + TGR_ADJUSTER)) / (float)TGR_LIMIT;
    return (int)((float)CPWM_DEADTIME + (float)TGR_ADJUSTER + e * (float)tgr_value);
}

float convert_to_current(int ad)
{
    return 2.0f * (float)MAX_CURRENT * (float)ad / 1024.0f - (float)MAX_CURRENT;
}

float convert_to_radian(int ad)
{
    return 2.0f * PI * ((float)ad / 1024.0f) / 3.0f;
}

int convert_to_tgr(float voltage)
{
    float v_range = VOLTAGE_MAX - VOLTAGE_MIN;

    if (voltage < VOLTAGE_MIN)
    {
        voltage = VOLTAGE_MIN;
    }
    else if (VOLTAGE_MAX < voltage)
    {
        voltage = VOLTAGE_MAX;
    }

    return (int)(((voltage - VOLTAGE_MIN) / v_range) * (float)TGR_LIMIT);
}

tgr_values vc_intr(void)
{
    float i_u = 0.0f;      // U���d���l
    float i_v = 0.0f;      // V���d���l
    float i_w = 0.0f;      // W���d���l
    float i_dc = 0.0f;     // DC�d���l
    float theta = 0.0f;    // ���[�^��]�p
    int acc = 0;           // �A�N�Z��
    int rot_direction = 1; // 1 = ���]; -1 = �t�]

    int ad_i_u = 0;
    int ad_i_v = 0;
    int ad_i_w = 0;
    int ad_i_dc = 0;
    int ad_acc = 0;
    int is_forward = 1;
    int resolver = 0;
    float i_alpha = 0.0f;
    float i_beta = 0.0f;
    float i_d = 0.0f;
    float i_q = 0.0f;
    float i_d_ref = 0.0f;
    float i_q_ref = 0.0f;
    float delta_i_d = 0.0f;
    float delta_i_q = 0.0f;
    float v_d = 0.0f;
    float v_q = 0.0f;
    float v_alpha = 0.0f;
    float v_beta = 0.0f;
    float v_u = 0.0f;
    float v_v = 0.0f;
    float v_w = 0.0f;
    int tgr_u = 0;
    int tgr_v = 0;
    int tgr_w = 0;
    tgr_values tgr_vals;

    /****************************
     *    Read Sensor Values    *
     ****************************/
    // AD�ϊ��J�n
    AD0.ADCR.BIT.ADST = 1;
    AD1.ADCR.BIT.ADST = 1;

    // AD�ϊ��I���܂őҋ@
    while (AD0.ADCSR.BIT.ADF == 0 || AD1.ADCSR.BIT.ADF == 0)
    {
    }

    // �� ����H
    AD0.ADCSR.BIT.ADF = 0;
    AD1.ADCSR.BIT.ADF = 0;

    ad_i_u = AD0.ADDR0 >> 6;
    ad_i_v = AD0.ADDR1 >> 6;
    ad_i_w = AD0.ADDR2 >> 6;
    ad_i_dc = AD0.ADDR3 >> 6;
    // ad_theta = AD1.ADDR4 >> 6;
    ad_acc = AD1.ADDR4 >> 6;
    is_forward = PA.PRL.BIT.B0;
    // resolver = PA.PRL.WORD & 0x03FF;
    resolver = (PA.PRL.WORD & 0x7FE0) >> 5;

    // i_u = convert_to_current(AD0.ADDR0 >> 6);
    // i_v = convert_to_current(AD0.ADDR1 >> 6);
    // i_w = convert_to_current(AD0.ADDR2 >> 6);
    i_u = convert_to_current(ad_i_u);
    i_v = convert_to_current(ad_i_v);
    i_w = convert_to_current(ad_i_w);
    i_dc = convert_to_current(ad_i_dc);
    // theta = convert_to_radian(AD1.ADDR4 >> 6);
    // theta = convert_to_radian(ad_theta);
    theta = convert_to_radian(resolver);
    acc = 100 * ad_acc / 1024;

    // if (theta < PHI)
    // {
    //     PE.DRL.BIT.B0 = 0;
    //     PE.DRL.BIT.B1 = 0;
    // }
    // else if (theta < 2 * PHI)
    // {
    //     PE.DRL.BIT.B0 = 1;
    //     PE.DRL.BIT.B1 = 0;
    // }
    // else
    // {
    //     PE.DRL.BIT.B0 = 1;
    //     PE.DRL.BIT.B1 = 1;
    // }

    /*********************************
     *    UVW -> Alpha&Beta -> dq    *
     *********************************/
    i_alpha = i_u + i_v * cosf(PHI) + i_w * cosf(-PHI);
    i_beta = i_v + sinf(PHI) + i_w * sinf(-PHI);
    // i_d = i_beta * cosf(theta) - i_alpha * sinf(theta);
    // i_q = i_alpha * cosf(theta) + i_beta * sinf(theta);
    i_d = i_beta * cosf(theta) + i_alpha * sinf(theta);
    i_q = i_alpha * cosf(theta) - i_beta * sinf(theta);

    /*********************************
     *    PI Control (torque)        *
     *********************************/
    i_d_ref = 0.0f;                              // I_d ���z�l
    i_q_ref = (float)acc * (float)rot_direction; // I_q ���z�l

    delta_i_d = i_d - i_d_ref;
    delta_i_q = i_q - i_q_ref;
    integral_delta_i_d += delta_i_d;
    integral_delta_i_q += delta_i_q;

    //v_d = K_P * delta_i_d + K_I * integral_delta_i_d;
    //v_q = K_P * delta_i_q + K_I * integral_delta_i_q;
    v_d = K_P * delta_i_d;
    v_q = K_P * delta_i_q;

    /*********************************
     *    dq -> Alpha&Beta -> UVW    *
     *********************************/
    // v_alpha = v_d * cosf(theta) - v_q * sinf(theta);
    // v_beta = v_q * cosf(theta) + v_d * sinf(theta);
    v_alpha = v_q * cosf(theta) + v_d * sinf(theta);
    v_beta = v_d * cosf(theta) - v_q * sinf(theta);
    v_u = v_alpha * 2.0f / 3.0f;
    v_v = v_alpha / (-3.0f) + v_beta / sqrtf(3.0f);
    v_w = v_alpha / (-3.0f) - v_beta / sqrtf(3.0f);

    v_u = v_u * VOLTAGE_ADJUSTER_PERCENT / 100;
    v_v = v_v * VOLTAGE_ADJUSTER_PERCENT / 100;
    v_w = v_w * VOLTAGE_ADJUSTER_PERCENT / 100;

    tgr_u = adjust_tgr(convert_to_tgr(v_u));
    tgr_v = adjust_tgr(convert_to_tgr(v_v));
    tgr_w = adjust_tgr(convert_to_tgr(v_w));

    // update_cpwm_duty(tgr_u, tgr_v, tgr_w);
    tgr_vals.u = tgr_u;
    tgr_vals.v = tgr_v;
    tgr_vals.w = tgr_w;

    return tgr_vals;
}

void main(void)
{
    int pwm_working = 1;
    int dms_on = 0;
    int protection_working = 0;
    int init_tgr_u = 0;
    int init_tgr_v = (int)((float)TGR_LIMIT * ((sinf(PHI) + 1.0f) / 2.0f));
    int init_tgr_w = (int)((float)TGR_LIMIT * ((sinf(-PHI) + 1.0f) / 2.0f));
    unsigned int counter = 0;
    unsigned int t = 0;
    float theta = 0.0f;
    tgr_values tgr_vals;

    STB.CR4.BIT._AD0 = 0;
    STB.CR4.BIT._AD1 = 0;
    STB.CR4.BIT._MTU2 = 0;

    init_cpwm(CPWM_CARRIER_CYCLE, CPWM_DEADTIME, init_tgr_u, init_tgr_v, init_tgr_w);

    PFC.PAIORL.WORD = 0x7FE0;
    PFC.PEIORL.BIT.B0 = 1;
    PFC.PEIORL.BIT.B1 = 1;
    PFC.PEIORL.BIT.B3 = 0;
    PFC.PEIORL.BIT.B4 = 0;
    PFC.PEIORL.BIT.B6 = 0;
    PFC.PECRL1.BIT.PE0MD = 0;
    PFC.PECRL1.BIT.PE1MD = 0;
    PFC.PECRL1.BIT.PE3MD = 0;
    PFC.PECRL2.BIT.PE4MD = 0;
    PFC.PECRL2.BIT.PE6MD = 0;

    start_cpwm();

    while (1)
    {
        // if (++counter == 20000)
        // {
        //     counter = 0;
        //     t++;
        //     t %= 1024;
        //     tgr_vals = vc_intr_test(t);
        //     update_cpwm_duty(tgr_vals.u, tgr_vals.v, tgr_vals.w);
        // }

        dms_on = PE.PRL.BIT.B4;
        protection_working = PE.PRL.BIT.B6;
        PE.DRL.BIT.B0 = pwm_working;

        if (dms_on || protection_working)
        {
            if (pwm_working)
            {
                stop_cpwm();
                pwm_working = 0;
            }
        }
        else
        {
            if (!pwm_working)
            {
                start_cpwm();
                pwm_working = 1;
            }

            if (pwm_working)
            {
                tgr_vals = vc_intr();
                update_cpwm_duty(tgr_vals.u, tgr_vals.v, tgr_vals.w);
            }
        }
        // if (++counter == 8000)
        //     counter = 0;

        // theta = 2.0f * PI * (float)counter / 8000.0f;

        // update_cpwm_duty(
        //     adjust_tgr((int)((float)TGR_LIMIT * ((1.0f + sinf(theta)) / 2.0f))),
        //     adjust_tgr((int)((float)TGR_LIMIT * ((1.0f + sinf(theta)) / 2.0f))),
        //     adjust_tgr((int)((float)TGR_LIMIT * ((1.0f + sinf(theta)) / 2.0f))));
    }
}

#ifdef __cplusplus
void abort(void)
{
}
#endif