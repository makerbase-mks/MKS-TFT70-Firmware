/********************   (C) COPYRIGHT 2014 www.makerbase.com.cn   ********************
 * �ļ���  ��MKS_USART2_IT.c
 * ����    ��Marlinͨ�Ŵ���ģ��
						 1. printerStaus = idle ,��PUSH��gcodeCmdTxFIFO�������usart2����,����usart2�յ���Ӧ��Push��gcodeCmdRxFIFO��
						 2. printerStaus = working��
						 		a) ʵʱ����gcodeTxFIFO�Ĵ�ӡ���ݣ�
						 		b) �������ݺ󣬳���5sδ�յ�Ӧ���ظ����͸����ݣ�ֱ���յ�Ӧ��
						 		c) �յ� Error:Line Number is not Last Line Number+1, Last Line: n������Nn+1���ݣ�
						 		d) �յ� Error:checksum mismatch, Last Line: n������Nn+1���ݡ�
						 		e) ��gcodeCmdTxFIFO������ʱ�����ȷ���gcodeCmdTxFIFO������,����Ӧ��Push��gcodeCmdRxFIFO��
						 3. �κ�ʱ��������󣬳���5sδ�յ�Ӧ���ظ����͸����ֱ���յ�Ӧ��
						 4. printerStaus״̬ת��ͼ����״̬ת��ͼ_pr��
 * ����    ��skyblue
**********************************************************************************/


#include "draw_ui.h"
#include "mks_tft_com.h"

USART2DATATYPE usart2Data;

TFT_FIFO gcodeTxFIFO;			//gcode ���ݷ��Ͷ���
//TFT_FIFO gcodeRxFIFO;			//gcode	���ݽ��ն���

TFT_FIFO gcodeCmdTxFIFO;		//gcode ָ��Ͷ���
TFT_FIFO gcodeCmdRxFIFO;		//gcode	ָ����ն���

__IO u16 tftDelayCnt = 0;
 
unsigned char pauseFlag = 0;
FIREWARE_TYPE firmwareType = marlin;
unsigned char wait_cnt = 0;

unsigned char positionSaveFlag = 0;


void mksUsart2Resend(void);

void mksUsart2Resend(void)
{
	//return;									//10s
		if(usart2Data.timerCnt>(10000/TICK_CYCLE)) 
		{
			if(usart2Data.timer == timer_running && usart2Data.prWaitStatus != pr_wait_idle)	//timer_running=1
			{
				switch(printerStaus)
					{
						case pr_working:	//pr_working = 1
						case pr_pause:		//pr_pause = 2
								//USART2_SR |= 0x0040;	//����һ�η����ж�
								usart2TxStart();	
							break;
						case pr_idle:		//pr_idle = 0
								usart2TxStart();	//0303
							break;							//0303
						case pr_stop:		//pr_stop = 3
								//usart2CmdTxStart();
							break;
						default : break;
					}
				}
			usart2Data.timerCnt = 0;	
		}
}

void mksUsart2RepeatTx(void)		
{
		usart2Data.timerCnt++;
	
		if(tftDelayCnt != 0)	tftDelayCnt--;
	
		//mksUsart2Resend();
	/*
		if(ADCConvertedValue < 0x0B60)  //4.7V
		{
			if(printerStaus == pr_working)	
			{
				BACKLIGHT = BACKLIGHT_OFF;	
				__set_PRIMASK(1);				//�ر����ж�
				__set_FAULTMASK(1);
				rePrintSaveData();
				__set_PRIMASK(0);			//�������ж�
				__set_FAULTMASK(0);
				BACKLIGHT = BACKLIGHT_ON;				
				
			}
		}
	*/
}

void mksUsart2Polling(void)
{
	unsigned char i;  //???

	mksBeeperAlarm();

	if(positionSaveFlag == 1 && printerStaus == pr_pause)		//�ƶ��󱣴�λ��
		{
		positionSaveFlag = 0;
		positionSaveProcess();
		}
	
	mksUsart2Resend();
	
	switch(printerStaus)
	{
		case pr_working:		//pr_working = 1
/*--------------reprint test--------------------*/	
				//if(mksReprintTest()) return;		
/*--------------reprint test--------------------*/	
				RePrintData.printerStatus = printer_normal;
				usart2Data.timer = timer_running;
				heatFlag = 0;
				if(pauseFlag == 1)
				{
					pauseFlag = 0;
					if(RePrintData.saveFlag == SAVE_DIS)
						relocalZ();
					else
						relacalSave();
				}
				
				
				switch(usart2Data.printer)
				{
					case printer_idle:				//printer_idle 0
								udiskBufferInit();
								usart2Data.printer = printer_working;
								while(checkFIFO(&gcodeTxFIFO)!= fifo_full)
								{
									udiskFileR(srcfp);												//���ļ�
									pushTxGcode();
									if(udiskFileStaus == udisk_file_end) 
									{
										while(printerStaus != pr_idle)
										{
											udiskFileR(srcfp);
										}
										break;
									}

								}
								printerInit();
								break;
					case printer_waiting:			//printer_waiting 2
								//relocalZ();
								usart2Data.printer = printer_working;
								while(checkFIFO(&gcodeTxFIFO)!= fifo_full)
								{
									udiskFileR(srcfp);												//���ļ�
									pushGcode();
									if(udiskFileStaus == udisk_file_end) 
									{
										while(printerStaus != pr_idle)
										{
											udiskFileR(srcfp);
										}
										break;
									}	
								}
								if(popFIFO(&gcodeTxFIFO,&usart2Data.usart2Txbuf[0]) != fifo_empty)	//��������
								{
									usart2Data.prWaitStatus = pr_wait_data;
									usart2TxStart();
								}
						break;
					case printer_working:	//printer_working  1
									udiskFileR(srcfp);
									pushGcode();			
						break;
					default :break;
				}
			break;
		case pr_pause:	//pr_pause = 2
				usart2Data.timer = timer_stop;
		
				if(homeXY()) 	pauseFlag = 1;
				rePrintSaveData();
				if(checkFIFO(&gcodeCmdTxFIFO) !=fifo_empty && usart2Data.prWaitStatus == pr_wait_idle)
						{
							popFIFO(&gcodeCmdTxFIFO,&usart2Data.usart2Txbuf[0]);
							usart2Data.prWaitStatus = pr_wait_cmd;
							usart2TxStart();
						}
					break;
		case pr_idle:		//pr_idle = 0
					if(checkFIFO(&gcodeCmdTxFIFO) !=fifo_empty && usart2Data.prWaitStatus == pr_wait_idle)
						{
							popFIFO(&gcodeCmdTxFIFO,&usart2Data.usart2Txbuf[0]);
							usart2Data.prWaitStatus = pr_wait_cmd;
							usart2Data.timer = timer_running;	//0303
							usart2TxStart();
						}
					break;
		case pr_stop:		//pr_stop = 3
					
					
					udiskFileStaus = udisk_file_end;
					printerStaus = pr_idle;		//��ӡ����
					usart2Data.printer = printer_idle;
					usart2Data.prWaitStatus = pr_wait_idle;
					usart2Data.timer = timer_stop;						//�����ʱ��
					
					//tftDelay(3);
					//printerInit();
					//tftDelay(2);
					I2C_EE_BufferRead(&dataToEeprom, BAK_REPRINT_INFO,  4);
					dataToEeprom &= 0x00ffffff;
					dataToEeprom |= (uint32_t)(printer_normal << 24 ) & 0xff000000;
					I2C_EE_BufferWrite(&dataToEeprom, BAK_REPRINT_INFO,  4); 		// �����־(uint8_t) | ��λunit (uint8_t) | saveFlag(uint8_t)| null(uint8_t)
		
					printerStop();

					break;
		
		case pr_reprint:		//����
				rePrintProcess();
				//printerStaus = pr_working;		//print test
				//usart2Data.printer = printer_waiting;
			break;
			
		default : break;
			
	}
}


void mksUsart2Init(void)
{
		firmwareType = marlin;
		wait_cnt = 0;
	
		usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
		usart2Data.txP = &usart2Data.usart2Txbuf[0];
		usart2Data.txBase = usart2Data.txP;
		usart2Data.printer = printer_idle;
		usart2Data.timer = timer_stop;
		usart2Data.prWaitStatus = pr_wait_idle;
		USART2_SR &= 0xffbf;		//����жϱ�־λ
		USART2_SR &= 0xffdf;
		USART2_CR1 |= 0x40;			//��������ж�����
		USART2_SR &= 0xffbf;		//����жϱ�־λ

		RePrintData.saveFlag = SAVE_DIS;
		
		udiskBufferInit();
	
		rePrintInit();
}

void mksUsart2IrqHandlerUser(void)
{
		unsigned char i;
		if( USART2_SR & 0x0020)	//rx 
		{
				*(usart2Data.rxP++) = USART2_DR & 0xff;
				USART2_SR &= 0xffdf;
			
				if(*(usart2Data.rxP-1) == '\n')		//0x0A �յ�������
				{
					if(RePrintData.saveEnable)	getSavePosition();
					
					if(usart2Data.usart2Rxbuf[0] =='w' &&  usart2Data.usart2Rxbuf[1] =='a' && usart2Data.usart2Rxbuf[2] =='i' &&  usart2Data.usart2Rxbuf[3] =='t')
					{	//repetier ȥ�����յ��� wait �ַ�
						usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
						wait_cnt++;
						if(wait_cnt > 2)
							firmwareType = repetier;
					}
					else
						{
							wait_cnt = 0;
							if(firmwareType != repetier)
								usart2Data.timerCnt = 0; //��ʱ������
							switch(printerStaus)
							{
								case pr_working:	//��ӡ�� pr_working = 1
								case pr_pause:  //��ͣ pr_pause = 2
										switch(usart2Data.prWaitStatus)
										{
											case pr_wait_idle:			//0
												pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);	//reretier
												break;
											case pr_wait_cmd:			//pr_wait_cmd=1 	������еȴ���Ӧ
												pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
												//M105:
												if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')
												{
													usart2Data.prWaitStatus = pr_wait_idle;
													prTxNext();
												}
												break;
											case pr_wait_data:

												if(firmwareType != repetier)
												{
													if(resendProcess()) break;
												}
												else
												{
													if(resendProcess_repetier()) break;
												}
												
												if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')	
												{
													if(recOkProcess()) 
													{
														usart2Data.resendCnt = 0;
														usart2Data.prWaitStatus = pr_wait_idle;
														prTxNext();
													}
													else	//ok : T xxx
														pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
												}
												else //�յ�������push ��CMD���� ��OK
													pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
												break;
											default : break;
										} //switch(usart2Data.prWaitStatus) 
									break;
								case pr_idle:		//	pr_idle=0				//�Ǵ�ӡ�� ,�����������ⲿ��ѯgcodeCmdTxFIFO�ǿգ���������
								case pr_stop:		//	pr_stop=3
										if(usart2Data.usart2Rxbuf[0] =='o' &&  usart2Data.usart2Rxbuf[1] =='k')
										{
											usart2Data.prWaitStatus = pr_wait_idle;
											usart2Data.timer = timer_stop;		//0303
										}
										pushFIFO(&gcodeCmdRxFIFO,&usart2Data.usart2Rxbuf[0]);
										break;							
								default :break;															
								}//switch(printerStaus)

								usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
							//memset(&usart2Data.usart2Rxbuf[0],0,sizeof(usart2Data.usart2Rxbuf));		//test_add
						}
				}
				if(usart2Data.rxP >= &usart2Data.usart2Rxbuf[0] + USART2BUFSIZE-1)
					usart2Data.rxP = &usart2Data.usart2Rxbuf[0];
			
		}

		
		if(USART2_SR & 0x0040)	//tx
		{
			USART2_SR &= 0xffbf;

			if(usart2Data.txP >= usart2Data.txBase + USART2BUFSIZE-1)
			{
				usart2Data.txP = usart2Data.txBase;
				return;
			}
			
			if(*usart2Data.txP != '\r')
				USART2_DR = *(usart2Data.txP++);

			if(*usart2Data.txP =='\n')
				*(usart2Data.txP+1) = '\r';
			
		}
	
}