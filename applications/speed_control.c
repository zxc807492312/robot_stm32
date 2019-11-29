#include "speed_control.h"

void speed_control(void *par);
static rt_thread_t tid_speed_control = RT_NULL;
struct rt_can_msg msg_send;
PID_TypeDef pid_yaw,pid_dist;
rt_int32_t test_u,test_e;
float ADRC_Unit[3][15]=
{
/*TD跟踪微分器   改进最速TD,h0=N*h      扩张状态观测器ESO(w0=4wc)           扰动补偿     			非线性组合(wc)*/
/*  r     h      	N0                 beta_01   beta_02    beta_03     	b0      	beta_0			beta_1      beta_2      alpha1  alpha2  zeta */
 {10000 ,0.005 , 	20,               	100,      	300,      1000,      	60,    		0.005,			400,      	20,     		0.8,   1.5,    0.03},
 {10000 ,0.005 , 	20,               	100,      	300,      1000,      	60,    		0.005,			400,      	20,     		0.8,   1.5,    0.03},
 {30000 ,0.005 , 	5,               	300,		4000,	  10000,      	100,   		0,				1.2,      	0.05,	    	0.8,   1.5,    0.03},
};

void speed_control_init(void)
{
	msg_send.id = 0x200;
    msg_send.ide = CAN_ID_STD;
    msg_send.rtr = CAN_RTR_DATA;
    msg_send.len = 0x08;

    msg_send.data[4] =  0;
    msg_send.data[5] = 	0;
    msg_send.data[6] =  0;
    msg_send.data[7] = 	0;
	ADRC_Init(ADRC_SPEED, ADRC_Unit,2);
	ADRC_Init(&ADRC_DEV, &ADRC_Unit[2],1);
	pid_init(&pid_yaw);
    pid_yaw.f_param_init(&pid_yaw,			//PID_TypeDef * pid
                              PID_Speed,				//PID_ID   id
                              1000,					//rt_uint16_t maxout
                              2000,						//rt_uint16_t intergral_limit
                              0,						//float deadband
                              0,						//rt_uint16_t period
                              2000,						//rt_int16_t  max_err
                              0,						//rt_int16_t  target
                              20,						//float 	kp
                              0,						//float 	ki
                              8);						//float 	kd
	pid_init(&pid_dist);
	pid_dist.f_param_init(&pid_dist,			//PID_TypeDef * pid
                              PID_Position,				//PID_ID   id
                              1000,					//rt_uint16_t maxout
                              1000,						//rt_uint16_t intergral_limit
                              0,						//float deadband
                              0,						//rt_uint16_t period
                              1000,						//rt_int16_t  max_err
                              0,						//rt_int16_t  target
                              4,						//float 	kp
                              0,						//float 	ki
                              0);	
    tid_speed_control = rt_thread_create("speed_control", speed_control, RT_NULL, 4096, 19, 20);

    if(RT_NULL != tid_speed_control)
        rt_thread_startup(tid_speed_control);
}

void speed_control(void *par)
{
	rt_uint8_t i;
    rt_int16_t ele[2];
	rt_int32_t real_v[2];
    rt_int32_t tar[2] = {0, 0},tar_next[2] = {0,0};
	rt_int32_t error = 0,dist_error=9999;
	rt_uint8_t done,status = 0;
	float u=0,tl,tr;
    while(1)
    {
        for(i = 0; i < 2; i++)
        {
			rt_mb_recv(&s_tar_mb[i], (rt_ubase_t *)&tar[i], RT_WAITING_NO);
        }
		for(i = 0; i < 2; i++)
        {
			rt_mb_recv(&s_nx_mb[i],(rt_ubase_t *)&tar_next[i],RT_WAITING_NO);
        }
		if(RT_EOK == rt_mb_recv(&s_error, (rt_ubase_t *)&error, RT_WAITING_NO))
		{
			//ADRC_Control(&ADRC_DEV,0,error/1000.0);
			pid_yaw.f_cal_pid(&pid_yaw, error/1000.0);
			u = pid_yaw.output / 2000.0;//限幅0.2
		}
		else
		{
			u=0;
		}
		if(RT_EOK == rt_mb_recv(&dist_err_mb, (rt_ubase_t *)&dist_error, RT_WAITING_NO))
		{
			pid_dist.f_cal_pid(&pid_dist, dist_error);
			pid_dist.output /= (1000.0);//转成比例,从1减小到0
		}
		if(pid_dist.output <0.3)//防止最后的低速爬行
		{
			pid_dist.output = 0.3;
		}
		tl = (tar[0]+(1-pid_dist.output)*(tar_next[0]-tar[0]))*(1+u);
		tr = (tar[1]+(1-pid_dist.output)*(tar_next[1]-tar[1]))*(1-u);
		
		test_u = (rt_int32_t)pid_yaw.output;
		test_e = (rt_int32_t)error;
		
		if(RT_EOK == rt_mb_recv(&s_kf_mb[0],(rt_ubase_t *)&real_v[0],RT_WAITING_FOREVER))
		{
			rt_mb_recv(&s_kf_mb[1],(rt_ubase_t *)&real_v[1],RT_WAITING_FOREVER);
			ADRC_Control(&ADRC_SPEED[0],tl,real_v[0]);
			ADRC_Control(&ADRC_SPEED[1],tr,real_v[1]);
			ele[0] = (rt_int16_t)ADRC_SPEED[0].u;
			ele[1] = (rt_int16_t)ADRC_SPEED[1].u;
			rt_mb_send(&adrc_v1_mb[0],(rt_int32_t)ADRC_SPEED[0].v1);
			rt_mb_send(&adrc_v1_mb[1],(rt_int32_t)ADRC_SPEED[1].v1);
			done = 1;
		}
		
		if(done)
		{
			msg_send.data[0] =  ele[0] >> 8 ;
			msg_send.data[1] =  ele[0];
			msg_send.data[2] =  ele[1] >> 8 ;
			msg_send.data[3] =  ele[1];
			dev_can1.ops->sendmsg(&dev_can1, &msg_send, CAN_TXMAILBOX_0);
			done = 0;
		}
        rt_thread_delay(1);
    }
}

