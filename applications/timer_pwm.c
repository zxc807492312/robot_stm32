#include "timer_pwm.h"


struct rt_device_pwm *pwm_dev_left,*pwm_dev_right,*pwm_dev_top;
rt_uint32_t period, pulse_l,pulse_r,pulse_t;



void timer_pwm_entry(void *par)
{
	rt_uint32_t recved;
	while(1)
	{
		if(RT_EOK == rt_event_recv(&event_pwm, EVENT_PWM_LEFT|EVENT_PWM_RIGHT|EVENT_PWM_CAM, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &recved))	//如果收到dist事件则开始计算
        {  //LEFT抓，RIGHT放
			if(recved == EVENT_PWM_LEFT)
			{
				rt_pin_write(POWERCI_PIN, PIN_HIGH);
				rt_pwm_set(pwm_dev_top, PWM_TOP, period, 3800000);
				rt_thread_mdelay(1000);
				rt_pwm_set(pwm_dev_top, PWM_TOP, period, 4300000);
			}
			if(recved == EVENT_PWM_RIGHT)
			{
				rt_pin_write(POWERCI_PIN, PIN_LOW);
				rt_pwm_set(pwm_dev_top, PWM_TOP, period, 4000000);
				rt_thread_mdelay(100);
				rt_pwm_set(pwm_dev_top, PWM_TOP, period, 4300000);
			}
			if(recved == EVENT_PWM_CAM)
			{
				for(rt_uint32_t i=0;i<10;i++)
				{
					rt_pwm_set(pwm_dev_top, PWM_TOP, period, 3000000+i*100000);
					rt_thread_mdelay(100);
				}
			}
			
		}
		
	}
}

void timer_pwm_init(void)
{
	period = 20000000;  //单位ns
	pulse_l = 4700000;	/* PWM脉冲宽度值，单位为纳秒ns */
	pulse_r = 2700000;
	pulse_t = 2900000;
	pwm_dev_left = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
	rt_pwm_set(pwm_dev_left, PWM_LEFT, period, pulse_l);
	rt_pwm_enable(pwm_dev_left, PWM_LEFT);
	pwm_dev_right = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
	rt_pwm_set(pwm_dev_right, PWM_RIGHT, period, pulse_r);
	rt_pwm_enable(pwm_dev_left, PWM_RIGHT);
	
	pwm_dev_top = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
	rt_pwm_set(pwm_dev_top, PWM_TOP, period, pulse_t);
	rt_pwm_enable(pwm_dev_top, PWM_TOP);
	
	
	tid_timer_pwm = rt_thread_create("tid_timer_pwm",
                                   timer_pwm_entry, RT_NULL,
                                   2048 ,
                                   8 , 10);		//优先级高一些
    if(tid_timer_pwm != RT_NULL)
        rt_thread_startup(tid_timer_pwm);
}
