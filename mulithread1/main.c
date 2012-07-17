/*
    test2.c

    Yashich C/C++ ライブラリ テストプログラム 2 (複数スレッド版)

    作成: 2006-04-28 (Fri) ver. 1.00      Y.TSUNODA  tsuno@tsunoda.jpn.org
    改変: 2006-05-01 (Mon) ver. 1.01      Y.TSUNODA  tsuno@tsunoda.jpn.org
    改変: 2006-05-08 (Mon) ver. 1.02      シーマーク様での改変内容反映

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

											/*	スレッド１生成				*/
	hThread[0] = (HANDLE)_beginthreadex(NULL,0,thread1,NULL,0,&dwThreadId[0]);
											/*	スレッド２生成				*/
	hThread[1] = (HANDLE)_beginthreadex(NULL,0,thread2,NULL,0,&dwThreadId[1]);

											/*	スレッド１、２終了待ち		*/
	WaitForMultipleObjects(2,hThread,TRUE,INFINITE);

	CloseHandle(hThread[0]);				/*	ハンドルクローズ			*/
	CloseHandle(hThread[1]);				/*	ハンドルクローズ			*/

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
