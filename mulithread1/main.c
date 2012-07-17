/*
    test2.c

    Yashich C/C++ ���C�u���� �e�X�g�v���O���� 2 (�����X���b�h��)

    �쐬: 2006-04-28 (Fri) ver. 1.00      Y.TSUNODA  tsuno@tsunoda.jpn.org
    ����: 2006-05-01 (Mon) ver. 1.01      Y.TSUNODA  tsuno@tsunoda.jpn.org
    ����: 2006-05-08 (Mon) ver. 1.02      �V�[�}�[�N�l�ł̉��ϓ��e���f

 */


//#include <windows.h>
#include <process.h>
#undef YLIB_EXPORTS
#include "..\ylib\ylib.h"

#define usleep Sleep

unsigned int WINAPI thread1(LPVOID	pParam);
unsigned int WINAPI thread2(LPVOID	pParam);


int main()
{
	HANDLE	hThread[2];
	DWORD	dwThreadId[2];

											/*	�X���b�h�P����				*/
	hThread[0] = (HANDLE)_beginthreadex(NULL,0,thread1,NULL,0,&dwThreadId[0]);
											/*	�X���b�h�Q����				*/
	hThread[1] = (HANDLE)_beginthreadex(NULL,0,thread2,NULL,0,&dwThreadId[1]);

											/*	�X���b�h�P�A�Q�I���҂�		*/
	WaitForMultipleObjects(2,hThread,TRUE,INFINITE);

	CloseHandle(hThread[0]);				/*	�n���h���N���[�Y			*/
	CloseHandle(hThread[1]);				/*	�n���h���N���[�Y			*/

	return 0;
}


unsigned int WINAPI thread1(LPVOID	pParam)
{
//    ysetupSocket();
    ysendIntAverageMetric( "ylibAPI:Int Average", 123456 );
    ysendIntAverageMetric( "ylibAPI:Int Average", 623451 );
    usleep( 100 );
    ysendIntCountMetric  ( "ylibAPI:Int Counter", 999999 );
    usleep( 100 );
    ysendStringMetric    ( "ylibAPI:String", "This is test." );
    usleep( 100 );

    ysendTransactionStart( "ylibAPI 1", "11seamark11@test.jp", "http://www.xxx.yyy", "query" );
    usleep( 100 );
		ysendTransactionEnter( "ylibAPI 1", "transaction enter" );
    usleep( 100 );
		ysendTransactionLeave( "ylibAPI 1", "transaction leave" );
    usleep( 100 );
		ysendTransactionEnter( "ylibAPI 1", "transaction enter" );
    usleep( 100 );
		ysendTransactionLeave( "ylibAPI 1", "transaction leave" );
    usleep( 100 );
    ysendTransactionEnd();
    usleep( 100 );
    //ysendTransactionEnter( "ylibAPI 1", "transaction enter" );
    //usleep( 100 );
    //ysendTransactionLeave( "ylibAPI 1", "transaction leave" );
    //usleep( 100 );
    ysendTransactionStart( "ylibAPI 1", "12seamark12@test.jp", "http://www.xxx.yyy", "query" );
    usleep( 100 );
		ysendTransactionEnter( "ylibAPI 1", "transaction enter" );
    usleep( 100 );
		ysendTransactionLeave( "ylibAPI 1", "transaction leave" );
    usleep( 100 );
    ysendTransactionEnd();
//    ycloseSocket();
	return 0;
}


unsigned int WINAPI thread2(LPVOID	pParam)
{
//    ysetupSocket();
	ysendIntAverageMetric( "ylibAPI 2:Average", 123456 );
	ysendIntAverageMetric( "ylibAPI 2:Average", 623451 );
    usleep( 100 );
    ysendIntCountMetric  ( "ylibAPI 2", 999999 );
    usleep( 100 );
    ysendStringMetric    ( "ylibAPI 2", "This is test." );
    usleep( 100 );
    ysendTransactionStart( "ylibAPI 2", "21seamark21@test.jp", "http://www.xxx.yyy", "query" );
    usleep( 100 );
		ysendTransactionEnter( "ylibAPI 2", "transaction enter" );
    usleep( 100 );
		ysendTransactionLeave( "ylibAPI 2", "transaction leave" );
    usleep( 100 );
		ysendTransactionEnter( "ylibAPI 2", "transaction enter" );
    usleep( 100 );
		ysendTransactionLeave( "ylibAPI 2", "transaction leave" );
    usleep( 100 );
    ysendTransactionEnd();
    usleep( 100 );
    //ysendTransactionEnter( "ylibAPI 2", "transaction enter" );
    usleep( 100 );
    //ysendTransactionLeave( "ylibAPI 2", "transaction leave" );
    usleep( 100 );
    ysendTransactionStart( "ylibAPI 2", "22seamark22@test.jp", "http://www.xxx.yyy", "query" );
    usleep( 100 );
		ysendTransactionEnter( "ylibAPI 2", "transaction enter" );
    usleep( 100 );
		ysendTransactionLeave( "ylibAPI 2", "transaction leave" );
    usleep( 100 );
    ysendTransactionEnd();
 //   ycloseSocket();
	return 0;
}
