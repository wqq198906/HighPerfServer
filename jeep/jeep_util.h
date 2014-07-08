/*
 * JEEP: Johan Efficient Event Platform
 *
 * jeep_util.h
 *	
 *  名称：工具方法
 *  功能：提供一些常用方法
 *  
 *  Created on: 2011-11-1
 *      Author: fengjianhua
 *        Mail: jianhuafeng@sohu-inc.com 56683216@qq.com
 *
 *  修改记录：
 */

#ifndef JEEP_UTIL_H_
#define JEEP_UTIL_H_

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "jeep_type.h"
#include <signal.h>

/*
 * 纳秒sleep
 */
void nsleep(uint32 ns);

/*
 * 秒和微妙sleep
 */
void susleep(uint32 s, uint32 us);

/*
 输入:工作路径
 功能:设置程序后台工作
 */
void daemonize();

/*
 输入:信号
 功能:设置信号集合
 */
typedef void (*sighandler_t)(int);
void signal_handler(int sig, sighandler_t handler);

#endif /* JEEP_UTIL_H_ */
