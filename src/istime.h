//
// Created by suitm on 2020/12/26.
//

#ifndef ISTIME_H
#define ISTIME_H

#define ISTIME_VERSION 20201226

/** ȡ��ǰʱ��ĺ����� **/
uint64_t istime_us();

/** ȡ��ʱʱ������� **/
uint64_t istime_s();

/**  ȡ��ǰʱ�����������ע��ó���500����ʧЧ����Ϊ�ᳬ��uint64����󳤶� **/
uint64_t istime_ns();

/** ��ȡ���յ�ʱ����Ϣ��6λ�� **/
uint64_t istime_time();

/** ��ȡ���յ����ڣ�8λ�� **/
uint64_t istime_date();

/** ��ȡ���յ�����+ʱ�䣬 14λ�� **/
uint64_t istime_datetime();

#endif //ISTIME_H
