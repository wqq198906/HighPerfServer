/*
 * akg.h
 *	
 *  ���ܣ���������ع���
 *  
 *  Created on: 2011-11-3
 *      Author: fengjianhua
 *        Mail: jianhuafeng@sohu-inc.com 56683216@qq.com
 *
 *  �޸ļ�¼��
 */

#ifndef STATS_AKG_H_
#define STATS_AKG_H_

#include "../jeep/jeep.h"
#include "conf.h"


int admin_user_code(CONN* c);
int check_user_code(CONN* c);
int check_upgrade(CONN* c);
int report_upgrade(CONN* c);

#endif /* STATS_AKG_H_ */
