// ylibtest2.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "..\ylib\ylib.h"

#define usleep Sleep

int _tmain(int argc, _TCHAR* argv[])
{
    ysetupSocket();

    ysendIntAverageMetric( "ylibAPI:AverageMetric", 12345 );
    ysendIntCountMetric  ( "ylibAPI:CountMetric", 99999 );
    ysendStringMetric    ( "ylibAPI:StringMetric", "This is test." );
    usleep( 100 );

    ysendTransactionStart( "ylibAPI", "test1@test.jp", "http://www.xxx.yyy", "query" );
    usleep( 100 );
    ysendTransactionEnter( "ylibAPI", "1transaction enter" );
    usleep( 100 );
    ysendTransactionLeave( "ylibAPI", "1transaction leave" );
    usleep( 100 );
    ysendTransactionEnter( "ylibAPI", "2transaction enter" );
    usleep( 10 );
    ysendTransactionLeave( "ylibAPI", "2transaction leave" );
    usleep( 100 );
    ysendTransactionEnd  ();


    //ysendTransactionEnter( "ylibAPI", "transaction enter" );
    //ysendTransactionLeave( "ylibAPI", "transaction leave" );
    usleep( 100 );
    ysendTransactionStart( "ylibAPI", "test2@test.jp", "http://www.xxx.yyy", "query" );
    usleep( 100 );
    ysendTransactionEnter( "ylibAPI", "DB transaction enter" );
    usleep( 100 );
    ysendTransactionLeave( "ylibAPI", "DB transaction leave" );
    usleep( 100 );
    ysendTransactionEnd();
    usleep( 100 );

    ysendTransactionStart( "ylibAPI", "test2@test.jp", "http://www.xxx.yyy", "test=123" );
    usleep( 100 );
    ysendTransactionEnter( "ylibAPI", "DB transaction enter" );
    usleep( 100 );
    ysendTransactionLeave( "ylibAPI", "DB transaction leave" );
    usleep( 100 );
    ysendTransactionEnd();

    ycloseSocket();
}

