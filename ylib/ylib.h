#ifndef _YLIB_H_
#define _YLIB_H_
/*
    ylib.h

    Yashich C/C++ ライブラリ ヘッダファイル
    Apache+PHPの性能情報を Yashichi に送信する API

    [API一覧]
      int   ysetupSocket          ();
      void  ycloseSocket          ();
      int   ysendMessage          ( char* str );

      int   ysendIntCountMetric          ( char* name, int value );
      int   ysendIntAverageMetric        ( char* name, int value );
      int   ysendLongCountMetric         ( char* name, char* numstr );
      int   ysendLongAverageMetric       ( char* name, char* numstr );
      int   ysendStringMetric            ( char* name, char* str );
      int   ysendPerIntervalCounterMetric( char* name );
      int   ysendIntRateCounterMetric    ( char* name );
      int   ysendTimeStampMetric         ( char* name );

      int   ysendTransactionStart ( char* name, char* uid, char* url, char* qStr );
      int   ysendTransactionStart2 ( char* name, char* uid, char* url, char* qStr, char* tid );
      int   ysendTransactionStart3 ( char* name, char* uid, char* url, char* qStr, char* tid, char* hid );
      int   ysendTransactionEnter ( char* name, char* str );
      int   ysendTransactionEnter2 ( char* name, char* str, char* tid );
      int   ysendTransactionEnter3 ( char* name, char* str, char* tid, char* hid );
      int   ysendTransactionLeave ( char* name, char* str );
      int   ysendTransactionLeave2 ( char* name, char* str, char* tid );
      int   ysendTransactionLeave3 ( char* name, char* str, char* tid, char* hid );
      int   ysendTransactionEnd   ();
      int   ysendTransactionEnd2   ( char* tid );
      int   ysendTransactionEnd3   ( char* tid, char* hid );
      int   ysetSendStatus        ( int );
      int   ygetSendStatus        ();
      int   ysetDestination       ( char* str );
      char* ygetDestination       ();
      long  ygetTransactionId();
      long  ygetGUID();


 */

#ifdef __cplusplus
#define BEGIN_EXTERN_C() extern "C" {
#define END_EXTERN_C() }
#else
#define BEGIN_EXTERN_C()
#define END_EXTERN_C()
#endif

#ifdef WIN32
#define YLIB_API	__stdcall 
# ifdef YLIB_EXPORTS
#	define YLIB_EXPORT __declspec(dllexport)
# else
#	define YLIB_EXPORT __declspec(dllimport)
# endif
#else
#define YLIB_API
#define SOCKET int
#endif

#ifdef WIN32

#include <malloc.h>
#include <stdlib.h>
#include <crtdbg.h>

#include <string.h>
#include <winsock2.h>
#define pthread_self	GetCurrentThread
#define LOG_ERR			stderr
#define LOG_DEBUG		stderr
#define LOG_WARNING		stderr
#define syslog			fprintf
//#define strcpy(dst,src)			strcpy_s(dst,sizeof(dst),src)
#define	snprintf		_snprintf
#define strtok_r(a,b,c)		strtok(a,b)

#define bcopy(x,y,n)	memcpy(y, x, (size_t)(n))
#define bzero(x,n)		memset(x, 0, (size_t)(n))

#endif

BEGIN_EXTERN_C()

//==============================================================================
// ソケットセットアップ
// SOCKET ysetupSocket()
//    引数: なし
//    返値: SOCKET 送信先のディスクリプタ (失敗時は負の整数)
//

SOCKET YLIB_EXPORT YLIB_API ysetupSocket();


//=============================================================================
// ソケットクローズ
// int ycloseSocket()
//    引数: なし
//    返値: なし
//

void YLIB_EXPORT YLIB_API ycloseSocket();


//=============================================================================
// メッセージ送信 (送信先へ文字列を送信)
// int ycloseSocket( char* str )
//    引数: str 送信文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//

int ysendMessage( char* str );


//=============================================================================
// 単独メトリック送信 (IC)
// int ysendIntCountMetric( char* name, int value )
//    引数: name  計測名文字列へのポインタ
//          value 計測値
//    返値: int 送信文字数 または エラー値(負の整数)
//



int YLIB_EXPORT YLIB_API ysendIntCountMetric( char* name, int value );



//=============================================================================
// 単独メトリック送信 (IA)
// int ysendIntAverageMetric( char* name, int value )
//    引数: name  計測名文字列へのポインタ
//          value 計測値
//    返値: int 送信文字数 または エラー値(負の整数)
//

int  YLIB_EXPORT YLIB_API ysendIntAverageMetric( char* name, int value );



//=============================================================================
// 単独メトリック送信 (LC)
// int ysendLongCountMetric( char* name, char* numstr )
//    引数: name   計測名文字列へのポインタ
//          numstr 計測値文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.06 で追加 suzuki@seamark.co.jp>

int  YLIB_EXPORT YLIB_API ysendLongCountMetric( char* name, char* numstr );



//=============================================================================
// 単独メトリック送信 (LA)
// int ysendLongAverageMetric( char* name, char* numstr )
//    引数: name   計測名文字列へのポインタ
//          numstr 計測値文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.06 で追加 suzuki@seamark.co.jp>

int  YLIB_EXPORT YLIB_API ysendLongAverageMetric( char* name, char* numstr );



//=============================================================================
// 単独メトリック送信 (SR)
// int ysendStringMetric( char* name, char* str )
//    引数: name 計測名文字列へのポインタ
//          str  計測値文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_EXPORT YLIB_API ysendStringMetric( char* name, char* str );



//=============================================================================
// 単独メトリック送信 (PC)
// int ysendPerIntervalCounterMetric( char* name )
//    引数: name 計測名文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.07 で追加 suzuki@seamark.co.jp>

int YLIB_EXPORT YLIB_API ysendPerIntervalCounterMetric( char* name );



//=============================================================================
// 単独メトリック送信 (IR)
// int ysendIntRateCounterMetric( char* name )
//    引数: name 計測名文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.07 で追加 suzuki@seamark.co.jp>

int YLIB_EXPORT YLIB_API ysendIntRateCounterMetric( char* name );



//=============================================================================
// 単独メトリック送信 (TI)
// int ysendTimeStampMetric( char* name )
//    引数: name 計測名文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.07 で追加 suzuki@seamark.co.jp>

int YLIB_EXPORT YLIB_API ysendTimeStampMetric( char* name );



//=============================================================================
// トランザクション送信 (TS)
// int ysendTransactionStart( char* name, char* uid, char* url, char* queryString )
//    引数: name         計測名文字列へのポインタ
//          uid          UserID文字列へのポインタ
//          url          URL文字列へのポインタ
//          queryString  QueryString文字列へのポインタ
// int ysendTransactionStart3( char* name, char* uid, char* url, char* queryString, char* tid )
//    引数: name         計測名文字列へのポインタ
//          uid          UserID文字列へのポインタ
//          url          URL文字列へのポインタ
//          queryString  QueryString文字列へのポインタ
//          tid          TransactionID文字列へのポインタ
//                       NULLのとき自動発番
// int ysendTransactionStart3( char* name, char* uid, char* url, char* queryString, char* tid, char* hid )
//    引数: name         計測名文字列へのポインタ
//          uid          UserID文字列へのポインタ
//          url          URL文字列へのポインタ
//          queryString  QueryString文字列へのポインタ
//          tid          TransactionID文字列へのポインタ
//                       NULLのとき自動発番
//          aid          ホスト名追加文字列へのポインタ
// 返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_EXPORT YLIB_API ysendTransactionStart( char* name, char* uid, char* url, char* queryString );
int YLIB_EXPORT YLIB_API ysendTransactionStart2( char* name, char* uid, char* url, char* queryString, char* tid);
int YLIB_EXPORT YLIB_API ysendTransactionStart3( char* name, char* uid, char* url, char* queryString, char* tid, char* hid);


//=============================================================================
// トランザクション送信 (ST)
// int ysendTransactionEnter( char* name, char* str )
//    引数: name 計測名文字列へのポインタ
//          str  文字列へのポインタ
// int ysendTransactionEnter2( char* name, char* str, char* tid )
//    引数: name 計測名文字列へのポインタ
//          str  文字列へのポインタ
//          tid  TransactionID文字列へのポインタ
//               NULLのとき自動発番
// int ysendTransactionEnter3( char* name, char* str, char* tid, char* hid )
//    引数: name 計測名文字列へのポインタ
//          str  文字列へのポインタ
//          tid  TransactionID文字列へのポインタ
//               NULLのとき自動発番
//          aid  ホスト名追加文字列へのポインタ
// 返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_EXPORT YLIB_API ysendTransactionEnter( char* name, char* str );
int YLIB_EXPORT YLIB_API ysendTransactionEnter2( char* name, char* str, char* tid );
int YLIB_EXPORT YLIB_API ysendTransactionEnter3( char* name, char* str, char* tid, char* hid );


//==============================================================================
// トランザクション送信 (ED)
// int ysendTransactionLeave( char* name, char* str )
//    引数: name 計測名文字列へのポインタ
//          str  文字列へのポインタ
// int ysendTransactionLeave2( char* name, char* str, char* tid )
//    引数: name 計測名文字列へのポインタ
//          str  文字列へのポインタ
//          tid  TransactionID文字列へのポインタ
//               NULLのとき自動発番
// int ysendTransactionLeave3( char* name, char* str, char* tid, char* hid )
//    引数: name 計測名文字列へのポインタ
//          str  文字列へのポインタ
//          tid  TransactionID文字列へのポインタ
//               NULLのとき自動発番
//          aid  ホスト名追加文字列へのポインタ
// 返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_EXPORT YLIB_API ysendTransactionLeave( char* name, char* str );
int YLIB_EXPORT YLIB_API ysendTransactionLeave2( char* name, char* str, char* tid );
int YLIB_EXPORT YLIB_API ysendTransactionLeave3( char* name, char* str, char* tid, char* hid );


//==============================================================================
// トランザクション送信 (TE)
// int ysendTransactionEnd()
//    引数: なし
// int ysendTransactionEnd2(char* tid)
//    引数: tid  TransactionID文字列へのポインタ
//               NULLのとき自動発番
// int ysendTransactionEnd3(char* tid, char* hid)
//    引数: tid  TransactionID文字列へのポインタ
//               NULLのとき自動発番
//          aid  ホスト名追加文字列へのポインタ
// 返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_EXPORT YLIB_API ysendTransactionEnd();
int YLIB_EXPORT YLIB_API ysendTransactionEnd2( char* tid );
int YLIB_EXPORT YLIB_API ysendTransactionEnd3( char* tid, char* hid );


//==============================================================================
// 送信すべきフラグ(sendstatus)値 設定
// int ysetSendStatus( int val )
//    引数: val 設定値 ( 送信 : 1, 不送信 0 )
//    返値: int 設定値(設定後の値, つまり引数valと同値)
//
// <ver.1.03 で追加 suzuki@seamark.co.jp>

int YLIB_EXPORT YLIB_API ysetSendStatus( int val );


//==============================================================================
// 送信すべきフラグ(sendstatus)値 取得
// int ygetSendStatus()
//    引数: なし
//    返値: int  送信すべきフラグ(sendstatus)値 ( 送信 : 1, 不送信 0 )
//
// <ver.1.03 で追加 suzuki@seamark.co.jp>

int YLIB_EXPORT YLIB_API ygetSendStatus();


//==============================================================================
// 送信先設定 
// int ysetDestination( char* str )
//    引数: str 送信先  ( [通信方式]://[送信先IPアドレス(IPv4)]:[ポート番号] )
//    返値: int 送信先のディスクリプタ (失敗時は負の整数)
//
// <ver.1.05 で追加 suzuki@seamark.co.jp>

int YLIB_EXPORT YLIB_API ysetDestination( char* str );


//==============================================================================
// 送信先文字列取得
// char* ygetDestination()
//    引数: なし
//    返値: char* 送信先文字列へのポインタ
//             ( [通信方式]://[送信先IPアドレス(IPv4)]:[ポート番号] )
//
// <ver.1.05 で追加 suzuki@seamark.co.jp>

YLIB_EXPORT char* YLIB_API ygetDestination();

//==============================================================================
// TransactionId取得
// long ygetTransactionId()
//    引数: なし
//    返値: long 取得したTransactionId
//
// 2007.09.04追加
long YLIB_EXPORT YLIB_API ygetTransactionId();

END_EXTERN_C()

//==============================================================================
// GUID取得
// long ygetGUID()
//    引数: なし
//    返値: long 取得したGUID
// <ver.1.09 で追加>
long ygetGUID();

//==============================================================================
// 自ホスト名設定
// void _yset_str_local()
//    引数: なし
//    返値: なし
// <ver.1.10 で追加>
void _yset_str_local();

//==============================================================================
// 自ホスト名取得
// char* _yget_str_local()
//    引数: なし
//    返値: なし
// <ver.1.10 で追加>
char* _yget_str_local();

#endif /*  _YLIB_H_ */
