/*
    ylib.c

    Yashich C/C++ ライブラリ
    Apache+PHPの性能情報を Yashichi に送信する API

    ( ylib 関数仕様書 SMK-YLIB-001-i の仕様 )

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

      int   ysendTransactionStart  ( char* name, char* uid, char* url, char* qStr );
      int   ysendTransactionStart2 ( char* name, char* uid, char* url, char* qStr, char* tid );
      int   ysendTransactionStart3 ( char* name, char* uid, char* url, char* qStr, char* tid, char* hid );
      int   ysendTransactionEnter  ( char* name, char* str );
      int   ysendTransactionEnter2 ( char* name, char* str, char* tid );
      int   ysendTransactionEnter3 ( char* name, char* str, char* tid, char* hid );
      int   ysendTransactionLeave  ( char* name, char* str );
      int   ysendTransactionLeave2 ( char* name, char* str, char* tid );
      int   ysendTransactionLeave3 ( char* name, char* str, char* tid, char* hid );
      int   ysendTransactionEnd    ();
      int   ysendTransactionEnd2   ( char* tid );
      int   ysendTransactionEnd3   ( char* tid, char* hid );

      int   ysetSendStatus        ( int );
      int   ygetSendStatus        ();
      int   ysetDestination       ( char* str );
      char* ygetDestination       ();
      long  ygetTransactionId();
      long  ygetGUID();


    [ライブラリ内部使用関数]
      int                 _yinitialize          ();
      void                _yreset_seqNo         ( long val );
      long                _ycountup_seqNo       ();
      long                _yget_seqNo           ();
      int                 _yset_s_dest          ( SOCKET val );
      SOCKET              _yget_s_dest          ();
      int                 _yset_socket_type     ( int val );
      int                 _yget_socket_type     ();
      void                _yset_str_dest        ( char* str );
      char*               _yget_str_dest        ();
      char*               _yget_str_utctime     ( char* buff );
      void                _yset_saddr           ( struct sockaddr_in* s );
      struct sockaddr_in* _yget_saddr           ();
      void                _ytsd_buffer_key_init ();
      tsd_buffer*         _yget_tsd_buffer      ();
      static void         _destroy_tsd          ( void * tsd);
      void                _yreplace_str         ( char* str );
      void                _yset_str_local       ();
      char*               _yget_str_local       ();


 */
#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32

#include <syslog.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#endif
#ifdef WIN32
#include <time.h>
#endif

#include "ylib.h"

#ifdef PHP_ATOM_INC
#include "Zend/zend.h"
#define YMALLOC(s) 	emalloc(s)
#define YCALLOC(n,s) 	ecalloc((n),(s))
#define YFREE(p) 	efree(p)
//#define  __NOT_USE_TLS__
#else
#define YMALLOC(s) 	malloc(s)
#define YCALLOC(n,s) 	calloc((n),(s))
#define YFREE(p) 	free(p)
#ifndef WIN32
#define  __NOT_USE_TLS__
#endif
#endif

// 初期値設定ファイル
#define INIFILENAME     "ylib.ini"

// 送信最大文字列長 (既定値)
//   (指定優先度は，環境変数 > 初期値設定ファイル > この値↓ )
#define YSEND_MSGMAXLEN 1024

#define UTCTIME_LEN	32

#ifdef WIN32
#define HOSTNAME_LEN	MAX_COMPUTERNAME_LENGTH
#else
#define HOSTNAME_LEN	256
#endif

#define YLIB_TRUE              1
#define YLIB_FALSE             0

// tsd_buffer.s_dest に格納するフラグ値
//   本来は送信先ディスクリプタ値を格納・保持するが，
//   「未初期化」「YSEND_DESTINATIONが未設定」の場合を識別するため，
//    s_dest に負の整数値を代入し使用する。
//    <ver.1.03 で追加 suzuki@seamark.co.jp>
#ifdef WIN32
#define YLIB_DEST_UNINITIALIZE NULL     // 未初期化 (ソケットが未初期化)
#define YLIB_DEST_NOTSET       NULL     // YESND_DESTINATION 未設定
#else
#define YLIB_DEST_UNINITIALIZE -1     // 未初期化 (ソケットが未初期化)
#define YLIB_DEST_NOTSET       -2     // YESND_DESTINATION 未設定
#endif

// time_resolution ( UTC-time の精度 ) の値
//    <ver.1.03 で追加 suzuki@seamark.co.jp>
#define YLIB_RESOLUTION_MILI    1
#define YLIB_RESOLUTION_MICRO   2



//-----------------------------------------------------------------------------
// 変数宣言
int  max_msglen=YSEND_MSGMAXLEN;           // 送信文字列最大長
int  flag_tracemode=YLIB_FALSE;       // トレースモードフラグ  YLIB_TRUE / YLIB_FALSE
int  sendstatus=YLIB_FALSE;           // 送信すべきフラグ
int  time_resolution=YLIB_RESOLUTION_MILI;      // UTC-time の精度

char str_addr       [128]; // 送信先アドレス文字列


//-----------------------------------------------------------------------------
// スレッドローカルな変数を格納するための構造体

typedef struct _tsd_buffer 
{
    SOCKET  s_dest;               // 送信先のソケット(ディスクリプタ)
    int  socket_type;             // 通信方式 SOCK_DGRAM(udp) / SOCK_STREAM(tcp)
    char str_dest  [128];         // 送信先文字列
    char str_local [HOSTNAME_LEN];// 自ホスト名文字列
	long  tid;					  // thread_loacl_id
    long  seqNo;                  // トランザクション seqNo

    struct sockaddr_in saddr;

} tsd_buffer;

#ifndef WIN32
static  pthread_once_t   tsd_buffer_alloc_once = PTHREAD_ONCE_INIT ;
static  pthread_key_t    tsd_buffer_key ;
#else
DWORD tsd_buffer_key = TlsAlloc();
#endif

#ifdef __NOT_USE_TLS__
struct _tsd_buffer *gAccess=NULL;
#endif
//-----------------------------------------------------------------------------
// 内部使用関数群 プロトタイプ宣言

void                _yinitialize          ();
void                _yreset_seqNo         ( long val );
long                _ycountup_seqNo       ();
long                _yget_seqNo           ();

SOCKET              _yset_s_dest          ( SOCKET val );
SOCKET              _yget_s_dest          ();

int                 _yset_socket_type     ( int val );
int                 _yget_socket_type     ();

void                _yset_str_dest        ( char* str );
char*               _yget_str_dest        ();

//char*               _yget_str_utctime     ( char* );
void                _yget_str_utctime     ( char* );

void                _yset_saddr           ( struct sockaddr_in* s );
struct sockaddr_in* _yget_saddr           ();

void                _ytsd_buffer_key_init ();
tsd_buffer*         _yget_tsd_buffer      ();
static void 	    _destroy_tsd(void * tsd);
void                _yreplace_str         ( char* str );
#ifdef __NOT_USE_TLS__
static tsd_buffer* _getAccess();
#endif

// 20071022追加
void                _yset_str_local       ();
char*               _yget_str_local       ();



//==============================================================================
// GUID取得
// long ygetGUID()
//    引数: なし
//    返値: long 取得したGUID
//
long  ygetGUID()
{
    // 時刻を取得する
#ifndef WIN32
    struct timeval  tv;
    gettimeofday( &tv, NULL );
	long ret = tv.tv_sec*1000L + tv.tv_usec;

    return ret;
#endif

#ifdef WIN32
	SYSTEMTIME	time_val;
	GetLocalTime( &time_val );
	long ret = time_val.wSecond*1000L + time_val.wMilliseconds;
    return ret;
#endif

}

//==============================================================================
// TransactionId取得
// long ygetTransactionId()
//    引数: なし
//    返値: long 取得したTransactionId
//
long YLIB_API ygetTransactionId()
{
#ifndef WIN32
        return pthread_self()+getpid();
#else
	return _yget_tsd_buffer()->tid;
		//return pthread_self();
        //return ygetGUID();
#endif
}


//==============================================================================
// ソケットセットアップ
// int ysetupSocket()
//    引数: なし
//    返値: int 送信先のディスクリプタ (失敗時は負の整数)
//

SOCKET YLIB_API ysetupSocket()
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();

	
    //--------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE (不送信)ならば，
    // この ysetupSocket(), 自関数の処理を中断する。
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>

    if( sendstatus == YLIB_FALSE ) {
        return ( YLIB_DEST_UNINITIALIZE );

    }


    //--------------------------------------------------------------------------
    // 送信先ディスクリプタを確認
    SOCKET d = _yget_s_dest();

    // 送信先ディスクリプタに負の値(識別フラグなど)が設定されている場合の処理
    // ・正常にディスクリプタが 存在する場合，または YLIB_DEST_NOTSET の場合，
    //   何もしない。
    //    <ver.1.04 で追加 suzuki@seamark.co.jp>
#ifdef WIN32
	if ( d != NULL ) return d;
#else
    if( d > 0 || d == YLIB_DEST_NOTSET ) {

        return ( d );
    }
#endif


    //--------------------------------------------------------------------------
    // 送信先情報 ( YSEND_DESTINATION )

    char  value[BUFSIZ];
    char* s;
#ifndef WIN32
    char* brkt;
#endif

    char  str_host [BUFSIZ];
    char  str_port [16];

    memset(str_host, 0, BUFSIZ);
    memset(str_port, 0, 16);
    strncpy( value, _yget_str_dest(), 128);

    // YSEND_DESTINATION (初期設定ファイル, 環境変数) が設定されていなければ，
    if( strcmp( value, "" ) == 0 ) {

        syslog( LOG_ERR,
            "ysetupSocket() [%lu] YSEND_DESTINATION is not set.", pthread_self() );

        // tsd_buffer.s_dest (送信先ディスクリプタ値) に
        // 「YSEND_DESTINATION 未設定」を示す値を設定する。
        //    <ver.1.03 で追加 suzuki@seamark.co.jp>
        _yset_s_dest( YLIB_DEST_NOTSET );

        return ( YLIB_DEST_NOTSET );

    // YSEND_DESTINATION が設定されていれば，
    } else {

        s = strtok_r( value, ":/", &brkt );
        if( strcmp( s, "tcp" ) == 0 ) { _yset_socket_type( SOCK_STREAM ); }
        else                          { _yset_socket_type( SOCK_DGRAM  ); }
        s = strtok_r( NULL,  ":/", &brkt );
        strcpy( str_host, s );
        s = strtok_r( NULL,  ":/", &brkt );
        strcpy( str_port, s );
    }

    //--------------------------------------------------------------------------
    // ソケットオープン
    //
	SOCKET fd;
    //int fd;                     // ファイルディスクリプタ (テンポラリ)

    if(( fd = socket( AF_INET, _yget_socket_type(), 0 )) < 0 ) {

        syslog( LOG_ERR,
                "ysetupSocket() [%lu] socket() error. [fd:%d]/[%s]",
                pthread_self(), fd, _yget_str_dest() );

        return ( YLIB_DEST_UNINITIALIZE );
    }

    struct sockaddr_in saddr;
#ifndef WIN32
    struct hostent  hp;
#else
    struct hostent* result;
#endif

    bzero((char *)&saddr, sizeof(saddr));

    int errnop;
//Solaris10とLinuxでgethostbyname_rの引数の数、返り値の有無が違うため分岐
//  <ver.1.08で追加>
// Solaris10の場合
#ifdef SOLARIS2
    result = gethostbyname_r( str_host, (struct hostent*)&hp, value, sizeof(value), &errnop);
    if (result == NULL){

// Linuxの場合
#else

#ifdef WIN32
    if (( result = gethostbyname( str_host )) == NULL) {
		errnop = WSAGetLastError();
#else
    if ( gethostbyname_r( str_host, (struct hostent *)&hp, value, sizeof(value), 
		(struct hostent **)&result,&errnop) != 0 ){
#endif

#endif
        syslog( LOG_ERR,
                "ysetupSocket() [%lu] gethostbyname_r() error. [%d],[%s]",
                pthread_self(), errnop, _yget_str_dest() );

        fprintf( stderr, "No such host.errno=[%d]", errnop );
        return( YLIB_DEST_UNINITIALIZE );
    }

#ifndef WIN32
    bcopy( hp.h_addr, &saddr.sin_addr, hp.h_length );
#else
	bcopy( result->h_addr, &saddr.sin_addr, result->h_length );
#endif
    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons( atoi( str_port ) );


    // TCP (SOCK_STREAM) の場合，接続の手続き
    if( _yget_socket_type() == SOCK_STREAM ) {

        int ret;
        ret = connect( fd, (struct sockaddr *)&saddr, sizeof( struct sockaddr_in ) );

        if( ret < 0 ) {
            // 接続失敗
            syslog( LOG_ERR,
                "ysetupSocket() [%lu] connect() error. [%d],[%s]",
                pthread_self(), ret, _yget_str_dest() );

            return( YLIB_DEST_UNINITIALIZE );
        }
    }


    // ファイルディスクリプタをTSD領域に格納
    _yset_s_dest( fd );
    _yset_saddr( &saddr );


    //-------------------------------------------------------------------------
    // トレース出力 > syslog

    if( flag_tracemode ) {
        syslog( LOG_DEBUG, "ysetupSocket() [%lu] socket open. [%s]",
                pthread_self(), _yget_str_dest() );
    }

    return fd;
}


//----------------------------------------------------------------------------
// Thread Local Strageが利用できないときに利用
void ycleanup(){
#ifdef __NOT_USE_TLS__
	ycloseSocket();
	if( gAccess!=NULL) {
		YFREE(gAccess);
		gAccess = NULL;
	}
#endif
}
//=============================================================================
// ソケットクローズ
// int ycloseSocket()
//    引数: なし
//    返値: なし
//

void YLIB_API ycloseSocket()
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    // 送信先がセットアップされているかどうか確認する
    SOCKET s = _yget_s_dest();

#ifndef WIN32
    if( s < 0 ) {
#else
	if ( s == NULL ){
#endif
        // fprintf( stderr, "ソケットがセットアップされていません\n" );
        return;
    }

    // ソケットをクローズする
#ifndef WIN32
    close( s );
#else
	closesocket( s );
	WSACleanup();
#endif


    // 送信先情報の格納領域に未初期化を示す値を代入
    //   ↑つまり，sTSD領域が確保され，_initialize() が未実行という状態
    //    <ver.1.03 で変更 suzuki@seamark.co.jp>
    _yset_s_dest     ( 0 );
    _yset_socket_type( 0 );


    // トレース出力
    if( flag_tracemode ) {
        syslog( LOG_DEBUG, "ycloseSocket() [%lu] socket close. (%s)",
                   pthread_self(), _yget_str_dest() );
    }

    _yset_str_dest   ( "" );

    //------------------------------------------------------------------------------
    // システム記録プログラム(syslog)への接続終了

#ifndef WIN32
    closelog();
#endif
}




//=============================================================================
// メッセージ送信 (送信先へ文字列を送信)
// int ysendMessage( char* str )
//    引数: str 送信文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//

int ysendMessage( char* str )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    // 送信先ディスクリプタを取得
    SOCKET s = _yget_s_dest();

    //--------------------------------------------------------------------------
    // 送信先ディスクリプタに負の値(識別フラグなど)が設定されている場合の処理
    // ※ 分岐にswitch文を用いたいところですが....，
    //    ・ディスクリプタが YLIB_DEST_UNINITIALIZE の場合，
    //      ysetupSocket() を実施し，その後(メッセージ送信前に)
    //      YLIB_DEST_NOTSET であるかどうかのチェックを行いたいから。
    //    ・不等号でディスクリプタ値を比較したいから
    //    という理由で，if文を用い，またif文の組を別にして記述しました。
    //
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>

    if( s == YLIB_DEST_UNINITIALIZE ) {

        // 送信先ディスクリプタ値が未セットアップなので，
        // ysetupSocket()を呼び出してセットアップを実施する
        s = ysetupSocket();
    }


    if( s == YLIB_DEST_NOTSET ) {

        // YSEND_DESTINATION が未設定ならば，つまり YLIB_DEST_NOTSETならば，
        // 何もせずに戻る
        return ( s );

    } else if( s < YLIB_DEST_NOTSET ) {

        // ディスクリプタが不明な負の値の場合...
        // 不明な状態なので，syslog にメッセージを送信しておく
        syslog( LOG_ERR, "ysendMessage() [%lu] port error. (%s)",
                   pthread_self(), _yget_str_dest() );

        return ( s );
    }


    //-------------------------------------------------------------------------
    // 送信文字列長のチェック

    char  buf[BUFSIZ];

    memset( buf, 0, BUFSIZ );
    strncpy( buf, str, BUFSIZ-1 );

    // 送信文字列最大長を超えた場合，
    if( (int)strlen( str ) > max_msglen ) {

        buf[max_msglen] = '\0';
        syslog( LOG_WARNING,
                "ysendMessage() [%lu] message size is over.", pthread_self() );
    }



    //-------------------------------------------------------------------------
    // 文字列送信
    //   TCP/UDP ともに sendto() を使用
    //     sendto() は TCP(SOCK_STREAM)の場合，第5,第6引数を無視する。
    //     sendto() は 送信失敗時 -1 を返す

    int sent_len = 0; // 送信したバイト数

    sent_len = sendto( _yget_s_dest(), buf, strlen( buf ), 0, 
                  (struct sockaddr *)_yget_saddr(), sizeof( struct sockaddr_in ) );



    //-------------------------------------------------------------------------
    // トレース出力 > syslog

    if( flag_tracemode ) {
        syslog( LOG_DEBUG, "ysendMessage() [%lu] [%s]", pthread_self(), buf );
    }

    return sent_len;
}




//=============================================================================
// 単独メトリック送信 (IC)
// int ysendIntCountMetric( char* name, int value )
//    引数: name  計測名文字列へのポインタ
//          value 計測値
//    返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_API ysendIntCountMetric( char* name, int value )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendIntCountMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return ( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //    <ver.1.06 で追加 suzuki@seamark.co.jp>

    char* p_name;
#ifndef WIN32
    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );
#else
    p_name = (char*)YMALLOC( strlen( name ) + 1);
#endif

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendIntCountMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );


    // 不要な文字を削除
    _yreplace_str( p_name );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1,"%s\t%lu\t1\tIC\t%s\t%s\t%d",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name, 
             value );


    // 確保した領域を解放
    // <ver.1.06 で追加 suzuki@seamark.co.jp>
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendIntCountMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return ( sent_len );
}




//=============================================================================
// 単独メトリック送信 (IA)
// int ysendIntAverageMetric( char* name, int value )
//    引数: name  計測名文字列へのポインタ
//          value 計測値
//    返値: int 送信文字数 または エラー値(負の整数)
//

int  YLIB_API ysendIntAverageMetric( char* name, int value )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendIntAverageMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return ( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //    <ver.1.06 で追加 suzuki@seamark.co.jp>

    char* p_name;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendIntAverageMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );

    // 不要な文字を削除
    _yreplace_str( p_name );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1, "%s\t%lu\t1\tIA\t%s\t%s\t%d",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name,
             value );


    // 確保した領域を解放
    // <ver.1.06 で追加 suzuki@seamark.co.jp>
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );

    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendIntAverageMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}


//=============================================================================
// 単独メトリック送信 (LC)
// int ysendLongCountMetric( char* name, char* numstr )
//    引数: name   計測名文字列へのポインタ
//          numstr 計測値文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.06 で追加 suzuki@seamark.co.jp>

int YLIB_API ysendLongCountMetric( char* name, char* numstr )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //

    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendLongCountMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }

    // 引数 numstr へのポインタの確認
    if( numstr == NULL ) {
        syslog( LOG_ERR, "ysendLongCountMetric() [%lu] Error in argument 'numstr'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //

    char* p_name;
    char* p_numstr;

    p_name   = (char*)YCALLOC( strlen( name   ) + 1, sizeof( char ) );
    p_numstr = (char*)YCALLOC( strlen( numstr ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendLongCountMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }
    if( p_numstr == NULL ) {
        syslog( LOG_ERR, "ysendLongCountMetric() [%lu] Error at YCALLOC() 'numstr'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name,   name   );
    strcpy( p_numstr, numstr );

    // 不要な文字を削除
    _yreplace_str( p_name   );
    _yreplace_str( p_numstr );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //

    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1, "%s\t%lu\t1\tLC\t%s\t%s\t%s",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name,
             p_numstr );

    // 確保した領域を解放
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;
    if (p_numstr != NULL ) YFREE( p_numstr ); p_numstr=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //

    int sent_len;
    sent_len = ysendMessage( message );

    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendLongCountMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}



//=============================================================================
// 単独メトリック送信 (LA)
// int ysendLongAverageMetric( char* name, char* numstr )
//    引数: name   計測名文字列へのポインタ
//          numstr 計測値文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.06 で追加 suzuki@seamark.co.jp>

int YLIB_API ysendLongAverageMetric( char* name, char* numstr )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //

    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendLongAverageMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }

    // 引数 numstr へのポインタの確認
    if( numstr == NULL ) {
        syslog( LOG_ERR, "ysendLongAverageMetric() [%lu] Error in argument 'numstr'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //

    char* p_name;
    char* p_numstr;

    p_name   = (char*)YCALLOC( strlen( name   ) + 1, sizeof( char ) );
    p_numstr = (char*)YCALLOC( strlen( numstr ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendLongAverageMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }
    if( p_numstr == NULL ) {
        syslog( LOG_ERR, "ysendLongAverageMetric() [%lu] Error at YCALLOC() 'numstr'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name,   name   );
    strcpy( p_numstr, numstr );

    // 不要な文字を削除
    _yreplace_str( p_name   );
    _yreplace_str( p_numstr );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //

    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1,"%s\t%lu\t1\tLA\t%s\t%s\t%s",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name,
             p_numstr );

    // 確保した領域を解放
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;
    if (p_numstr != NULL ) YFREE( p_numstr ); p_numstr=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //

    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendLongAverageMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}



//=============================================================================
// 単独メトリック送信 (SR)
// int ysendStringMetric( char* name, char* str )
//    引数: name 計測名文字列へのポインタ
//          str  計測値文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//

int YLIB_API ysendStringMetric( char* name, char* str )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendStringMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }

    // 引数 str へのポインタの確認
    if( str == NULL ) {
        syslog( LOG_ERR, "ysendStringMetric() [%lu] Error in argument 'str'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //    <ver.1.06 で追加 suzuki@seamark.co.jp>

    char* p_name;
    char* p_str;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );
    p_str  = (char*)YCALLOC( strlen( str  ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendStringMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }
    if( p_str == NULL ) {
        syslog( LOG_ERR, "ysendStringMetric() [%lu] Error at YCALLOC() 'str'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );
    strcpy( p_str,  str  );

    // 不要な文字を削除
    _yreplace_str( p_name );
    _yreplace_str( p_str  );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1,"%s\t%lu\t1\tSR\t%s\t%s\t%s",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name,
             p_str );

    // 確保した領域を解放
    // <ver.1.06 で追加 suzuki@seamark.co.jp>
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;
    if (p_str != NULL ) YFREE( p_str ); p_str=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );

    
    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendStringMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}



//=============================================================================
// 単独メトリック送信 (PC)
// int ysendPerIntervalCounterMetric( char* name )
//    引数: name 計測名文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.07 で追加 suzuki@seamark.co.jp>

int YLIB_API ysendPerIntervalCounterMetric( char* name )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, 
                "ysendPerIntervalCounterMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する

    char* p_name;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, 
                "ysendPerIntervalCounterMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );

    // 不要な文字を削除
    _yreplace_str( p_name );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1,"%s\t%lu\t1\tPC\t%s\t%s",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name );

    // 確保した領域を解放
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR,
                "ysendPerIntervalCounterMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}


//=============================================================================
// 単独メトリック送信 (IR)
// int ysendIntRateCounterMetric( char* name )
//    引数: name 計測名文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.07 で追加 suzuki@seamark.co.jp>

int YLIB_API ysendIntRateCounterMetric( char* name )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR,
                "ysendIntRateCounterMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する

    char* p_name;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR,
                "ysendIntRateCounterMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );

    // 不要な文字を削除
    _yreplace_str( p_name );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1,"%s\t%lu\t1\tIR\t%s\t%s",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name );

    // 確保した領域を解放
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR,
                "ysendIntRateCounterMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}



//=============================================================================
// 単独メトリック送信 (TI)
// int ysendTimeStampMetric( char* name )
//    引数: name 計測名文字列へのポインタ
//    返値: int 送信文字数 または エラー値(負の整数)
//
// <ver.1.07 で追加 suzuki@seamark.co.jp>

int YLIB_API ysendTimeStampMetric( char* name )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR,
                "ysendTimeStampMetric() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する

    char* p_name;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR,
                "ysendTimeStampMetric() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );

    // 不要な文字を削除
    _yreplace_str( p_name );


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );

    snprintf( message, BUFSIZ-1,"%s\t%lu\t1\tTI\t%s\t%s",
             str_hostname,
             pthread_self(),
             ( utctime ),
             p_name );

    // 確保した領域を解放
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;


    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR,
                "ysendTimeStampMetric() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}



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

int YLIB_API ysendTransactionStart( char* name, char* uid, char* url, char* queryString )
{
        return ysendTransactionStart2( name, uid, url, queryString, NULL );
}
int YLIB_API ysendTransactionStart2( char* name, char* uid, char* url, char* queryString, char* tid )
{
        return ysendTransactionStart3( name, uid, url, queryString, tid, NULL );
}
int YLIB_API ysendTransactionStart3( char* name, char* uid, char* url, char* queryString, char* tid, char* hid )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();

    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }

    // 引数 uid へのポインタの確認
    if( uid == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error in argument 'UserID'",
                pthread_self() );
        return( -1 );
    }

    // 引数 url へのポインタの確認
    if( url == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error in argument 'URL'",
                pthread_self() );
        return( -1 );
    }

    // 引数 queryString へのポインタの確認
    if( queryString == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error in argument 'QueryString'",
                pthread_self() );
        return( -1 );
    }

    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //    <ver.1.06 で追加 suzuki@seamark.co.jp>

    char* p_name;
    char* p_uid;
    char* p_url;
    char* p_qs;
    char* p_tid;
    char* p_hid;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );
    p_uid  = (char*)YCALLOC( strlen( uid  ) + 1, sizeof( char ) );
    p_url  = (char*)YCALLOC( strlen( url  ) + 1, sizeof( char ) );
    p_qs   = (char*)YCALLOC( strlen( queryString ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }
    if( p_uid == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'UserID'",
                pthread_self() );
        return ( -1 );
    }
    if( p_url == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'URL'",
                pthread_self() );
        return ( -1 );
    }
    if( p_qs == NULL ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'QueryString'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );
    strcpy( p_uid,  uid );
    strcpy( p_url,  url );
    strcpy( p_qs,   queryString );

    // 不要な文字を削除
    _yreplace_str( p_name );
    _yreplace_str( p_uid  );
    _yreplace_str( p_url  );
    _yreplace_str( p_qs   );

    // tid対応
    // <ver.1.09で追加>
    // tidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( tid != NULL ) {
        p_tid  = (char*)YCALLOC( strlen( tid  ) + 1, sizeof( char ) );

        if( p_tid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'TransactionID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_tid,  tid );
        _yreplace_str( p_tid  );
    }

    // hid対応
    // <ver.1.10で追加>
    // hidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( hid != NULL ) {
        p_hid  = (char*)YCALLOC( strlen( hid  ) + 1, sizeof( char ) );

        if( p_hid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'HostID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_hid,  hid );
        _yreplace_str( p_hid  );
        
        if ( strlen( p_hid ) > 5 ) {
            syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error at YCALLOC() 'HostID Length'",
                    pthread_self() );
            return ( -1 );
        }
    }

    //-------------------------------------------------------------------------
    //  当該スレッドの SeqNo に 1 を代入する。
    _yreset_seqNo( 1 );

    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );
    if ( hid != NULL ) {
        strcat( str_hostname, p_hid );
    }

    if ( tid != NULL ) {
        snprintf( message, BUFSIZ-1,"%s\t%s\t%d\tTS\t%s\t%s\t\t%s\t%s\t%s",
                 str_hostname,
                 p_tid,
                 _yget_seqNo(),
                 ( utctime ),
                 p_name,
                 p_uid,
                 p_url,
                 p_qs );
    } else {
        snprintf( message, BUFSIZ-1,"%s\t%lu\t%d\tTS\t%s\t%s\t\t%s\t%s\t%s",
                 str_hostname,
                 ygetTransactionId(),
                 _yget_seqNo(),
                 ( utctime ),
                 p_name,
                 p_uid,
                 p_url,
                 p_qs );
    }

    // 確保した領域を解放
    // <ver.1.06 で追加 suzuki@seamark.co.jp>

    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;
    if (p_uid != NULL ) YFREE( p_uid ); p_uid=NULL;
    if (p_url != NULL ) YFREE( p_url ); p_url=NULL;
    if (p_qs != NULL ) YFREE( p_qs ); p_qs=NULL;
    // <ver.1.09で追加>
    if ( tid != NULL ) {
        if (p_tid != NULL ) YFREE( p_tid ); p_tid=NULL;
    }
    // <ver.1.10で追加>
    if ( hid != NULL ) {
        if (p_hid != NULL ) YFREE( p_hid ); p_hid=NULL;
    }

    //-------------------------------------------------------------------------
    // メッセージ送信
    //
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendTransactionStart() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}



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
int YLIB_API ysendTransactionEnter( char* name, char* str )
{
        return ysendTransactionEnter2( name, str, NULL );
}
int YLIB_API ysendTransactionEnter2( char* name, char* str, char* tid )
{
        return ysendTransactionEnter3( name, str, tid, NULL );
}
int YLIB_API ysendTransactionEnter3( char* name, char* str, char* tid, char* hid )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //-------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }

    // 引数 str へのポインタの確認
    if( str == NULL ) {
        syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error in argument 'str'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //    <ver.1.06 で追加 suzuki@seamark.co.jp>

    char* p_name;
    char* p_str;
    char* p_tid;
    char* p_hid;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );
    p_str  = (char*)YCALLOC( strlen( str  ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }
    if( p_str == NULL ) {
        syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error at YCALLOC() 'str'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );
    strcpy( p_str,  str  );

    // 不要な文字を削除
    _yreplace_str( p_name );
    _yreplace_str( p_str  );

    // tid対応
    // <ver.1.09で追加>
    // tidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( tid != NULL ) {
        p_tid  = (char*)YCALLOC( strlen( tid  ) + 1, sizeof( char ) );

        if( p_tid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error at YCALLOC() 'TransactionID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_tid,  tid );
        _yreplace_str( p_tid  );
    }

    // hid対応
    // <ver.1.10で追加>
    // hidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( hid != NULL ) {
        p_hid  = (char*)YCALLOC( strlen( hid  ) + 1, sizeof( char ) );

        if( p_hid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error at YCALLOC() 'HostID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_hid,  hid );
        _yreplace_str( p_hid  );
    }

    //-------------------------------------------------------------------------
    //  当該スレッドの SeqNo に 1 を加算する。
    // _ycountup_seqNo()
    //  ※) ylib 関数仕様書では SeqNo をカウントアップし，送信文字列生成時に，
    //     値を取得する手順であったが，実行速度を向上させる目的で TSD領域への
    //     アクセスを一度にする(アクセス回数を減らす)ため，下の送信文字列を
    //     生成する箇所でカウントアップと加算後の値の取得を同時に行っている。


    //-------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );
    if ( hid != NULL ) {
        strcat( str_hostname, p_hid );
    }

    if ( tid != NULL ) {
        snprintf( message, BUFSIZ-1,"%s\t%s\t%u\tST\t%s\t%s\t%s",
                 str_hostname,
                 p_tid,
                 _ycountup_seqNo(),                   // SeqNo 
                 ( utctime ),
                 p_name,
                 p_str );
    } else {
        snprintf( message, BUFSIZ-1,"%s\t%lu\t%u\tST\t%s\t%s\t%s",
                 str_hostname,
                 ygetTransactionId(),
                 _ycountup_seqNo(),                   // SeqNo 
                 ( utctime ),
                 p_name,
                 p_str );
    }

    // 確保した領域を解放
    // <ver.1.06 で追加 suzuki@seamark.co.jp>
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;
    if (p_str != NULL ) YFREE( p_str ); p_str=NULL;
    // <ver.1.09で追加>
    if ( tid != NULL ) {
        if (p_tid != NULL ) YFREE( p_tid ); p_tid=NULL;
    }
    // <ver.1.10で追加>
    if ( hid != NULL ) {
        if (p_hid != NULL ) YFREE( p_hid ); p_hid=NULL;
    }

    //-------------------------------------------------------------------------
    // メッセージ送信
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendTransactionEnter() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}

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
int YLIB_API ysendTransactionLeave( char* name, char* str )
{
        return ysendTransactionLeave2( name, str, NULL );
}
int YLIB_API ysendTransactionLeave2( char* name, char* str, char* tid )
{
        return ysendTransactionLeave3( name, str, tid, NULL );
}
int YLIB_API ysendTransactionLeave3( char* name, char* str, char* tid, char* hid )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }


    //--------------------------------------------------------------------------
    // 引数チェック
    //

    // 引数 name へのポインタの確認
    if( name == NULL ) {
        syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error in argument 'name'",
                pthread_self() );
        return( -1 );
    }

    // 引数 str へのポインタの確認
    if( str == NULL ) {
        syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error in argument 'str'",
                pthread_self() );
        return( -1 );
    }


    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    //    <ver.1.06 で追加 suzuki@seamark.co.jp>

    char* p_name;
    char* p_str;
    char* p_tid;
    char* p_hid;

    p_name = (char*)YCALLOC( strlen( name ) + 1, sizeof( char ) );
    p_str  = (char*)YCALLOC( strlen( str  ) + 1, sizeof( char ) );

    if( p_name == NULL ) {
        syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error at YCALLOC() 'name'",
                pthread_self() );
        return ( -1 );
    }
    if( p_str == NULL ) {
        syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error at YCALLOC() 'str'",
                pthread_self() );
        return ( -1 );
    }

    strcpy( p_name, name );
    strcpy( p_str,  str  );

    // 不要な文字を削除
    _yreplace_str( p_name );
    _yreplace_str( p_str  );

    // tid対応
    // <ver.1.09で追加>
    // tidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( tid != NULL ) {
        p_tid  = (char*)YCALLOC( strlen( tid  ) + 1, sizeof( char ) );

        if( p_tid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error at YCALLOC() 'TransactionID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_tid,  tid );
        _yreplace_str( p_tid  );
    }

    // hid対応
    // <ver.1.10で追加>
    // hidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( hid != NULL ) {
        p_hid  = (char*)YCALLOC( strlen( hid  ) + 1, sizeof( char ) );

        if( p_hid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error at YCALLOC() 'HostID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_hid,  hid );
        _yreplace_str( p_hid  );
    }

    //--------------------------------------------------------------------------
    //  当該スレッドの SeqNo に 1 を加算する。
    // _ycountup_seqNo()
    //  ※) ylib 関数仕様書では SeqNo をカウントアップし，送信文字列生成時に，
    //     値を取得する手順であったが，実行速度を向上させる目的で TSD領域への
    //     アクセスを一度にする(アクセス回数を減らす)ため，下の送信文字列を
    //     生成する箇所でカウントアップと加算後の値の取得を同時に行っている。


    //--------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );
    if ( hid != NULL ) {
        strcat( str_hostname, p_hid );
    }

    if ( tid != NULL ) {
        snprintf( message,BUFSIZ-1, "%s\t%s\t%u\tED\t%s\t%s\t%s",
                 str_hostname,
                 p_tid,
                 _ycountup_seqNo(),                   // SeqNo
                 ( utctime ),
                 p_name,
                 p_str );
    } else {
        snprintf( message,BUFSIZ-1, "%s\t%lu\t%u\tED\t%s\t%s\t%s",
                 str_hostname,
                 ygetTransactionId(),
                 _ycountup_seqNo(),                   // SeqNo
                 ( utctime ),
                 p_name,
                 p_str );
    }

    // 確保した領域を解放
    // <ver.1.06 で追加 suzuki@seamark.co.jp>
    if (p_name != NULL ) YFREE( p_name ); p_name=NULL;
    if (p_str != NULL ) YFREE( p_str ); p_str=NULL;
    // <ver.1.09 で追加>
    if ( tid != NULL ) {
        if (p_tid != NULL ) YFREE( p_tid ); p_tid=NULL;
    }
    // <ver.1.10 で追加>
    if ( hid != NULL ) {
        if (p_hid != NULL ) YFREE( p_hid ); p_hid=NULL;
    }


    //--------------------------------------------------------------------------
    // メッセージ送信
    int sent_len;
    sent_len = ysendMessage( message );


    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendTransactionLeave() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );

}


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
int YLIB_API ysendTransactionEnd()
{
        return ysendTransactionEnd2( NULL );
}
int YLIB_API ysendTransactionEnd2( char* tid )
{
        return ysendTransactionEnd3( tid, NULL );
}
int YLIB_API ysendTransactionEnd3( char* tid, char* hid )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();

    //-------------------------------------------------------------------------
    // 送信すべきフラグ sendstatus が YLIB_FALSE(不送信) ならば，
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    if( sendstatus == YLIB_FALSE ) {

        // 送信していないので 0 文字
        return ( 0 );
    }

    //-------------------------------------------------------------------------
    // 引数の文字列から不要な文字を除去する
    char* p_tid;
    char* p_hid;

    // tid対応
    // <ver.1.09で追加>
    // tidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( tid != NULL ) {
        p_tid  = (char*)YCALLOC( strlen( tid  ) + 1, sizeof( char ) );

        if( p_tid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionEnd() [%lu] Error at YCALLOC() 'TransactionID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_tid,  tid );
        _yreplace_str( p_tid  );
    }

    // hid対応
    // <ver.1.10で追加>
    // hidに文字列が指定されていれば不要文字除去処理。NULLの場合は何もしない。
    if ( hid != NULL ) {
        p_hid  = (char*)YCALLOC( strlen( hid  ) + 1, sizeof( char ) );

        if( p_hid == NULL ) {
            syslog( LOG_ERR, "ysendTransactionEnd() [%lu] Error at YCALLOC() 'HostID'",
                    pthread_self() );
            return ( -1 );
        }
        strcpy( p_hid,  hid );
        _yreplace_str( p_hid  );
    }

    //--------------------------------------------------------------------------
    //  当該スレッドの SeqNo に 1 を加算する。
    // _ycountup_seqNo()
    //  ※) ylib 関数仕様書では SeqNo をカウントアップし，送信文字列生成時に，
    //     値を取得する手順であったが，実行速度を向上させる目的で TSD領域への
    //     アクセスを一度にする(アクセス回数を減らす)ため，下の送信文字列を
    //     生成する箇所でカウントアップと加算後の値の取得を同時に行っている。

    //--------------------------------------------------------------------------
    // 送信文字列の生成
    //
    char message [BUFSIZ];
    memset( message, 0, BUFSIZ );

    // 時刻格納用
    // <ver.1.03 で追加 suzuki@seamark.co.jp>
    char utctime [UTCTIME_LEN];
    memset( utctime, 0, UTCTIME_LEN );_yget_str_utctime( utctime );

    // ホスト名を取得する
    char str_hostname [HOSTNAME_LEN];
    memset( str_hostname, 0, HOSTNAME_LEN );
    //gethostname( str_hostname, HOSTNAME_LEN-1 );
    strcpy( str_hostname,_yget_str_local() );
    if ( hid != NULL ) {
        strcat( str_hostname, p_hid );
    }

    if ( tid != NULL ) {
        snprintf( message, BUFSIZ-1,"%s\t%s\t%u\tTE\t%s",
                 str_hostname,
                 p_tid,
                 _ycountup_seqNo(),                   // SeqNo
                 ( utctime ) );
    } else {
        snprintf( message, BUFSIZ-1,"%s\t%lu\t%u\tTE\t%s",
                 str_hostname,
                 ygetTransactionId(),
                 _ycountup_seqNo(),                   // SeqNo
                 ( utctime ) );
    }

    // 確保した領域を解放
    // <ver.1.09 で追加>
    if ( tid != NULL ) {
        if (p_tid != NULL ) YFREE( p_tid ); p_tid=NULL;
    }
    // <ver.1.10 で追加>
    if ( hid != NULL ) {
        if (p_hid != NULL ) YFREE( p_hid ); p_hid=NULL;
    }

    //--------------------------------------------------------------------------
    // メッセージ送信
    int sent_len;
    sent_len = ysendMessage( message );

    if( sent_len < 0 && sent_len != YLIB_DEST_NOTSET ) {
        syslog( LOG_ERR, "ysendTransactionEnd() [%lu] Error in ysendMessage()",
                pthread_self() );
    }

    return( sent_len );
}


//==============================================================================
// 送信すべきフラグ(sendstatus)値 設定
// int ysetSendStatus( int val )
//    引数: val 設定値 ( 送信 : 1, 不送信 0 )
//    返値: int 設定値(設定後の値, つまり引数valと同値) 
//
//    ※ 注意： 引数で与えられる 送信/不送信の値と，内部で保持する sendstatus
//             の値は必ずしも同値ではありません。
//
// <ver.1.03 で追加 suzuki@seamark.co.jp>

int YLIB_API ysetSendStatus( int val )
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    if( val ) { 
        sendstatus = YLIB_TRUE;
    } else {
        sendstatus = YLIB_FALSE;
    } 

    if( flag_tracemode ) {
        syslog( LOG_DEBUG, "ysetSendStatus() [%lu] set SendStatus [%d].",
                pthread_self(), val );
    }

    return( val );
}


//==============================================================================
// 送信すべきフラグ(sendstatus)値 取得
// int ygetSendStatus()
//    引数: なし
//    返値: int  送信すべきフラグ(sendstatus)値 ( 送信 : 1, 不送信 0 )
//
//    ※ 注意： 引数で与えられる 送信/不送信の値と，内部で保持する sendstatus
//             の値は必ずしも同値ではありません。
//
// <ver.1.03 で追加 suzuki@seamark.co.jp>

int YLIB_API ygetSendStatus()
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();


    if( sendstatus == YLIB_TRUE ) { return ( 1 ); }
    else                          { return ( 0 ); }
}



//==============================================================================
// 送信先設定
// int ysetDestination( char* str )
//    引数: str 送信先  ( [通信方式]://[送信先IPアドレス(IPv4)]:[ポート番号] )
//    返値: int 送信先のディスクリプタ (失敗時は負の整数)
//
// <ver.1.05 で追加 suzuki@seamark.co.jp>

int YLIB_API ysetDestination( char* str )
{
    //--------------------------------------------------------------------------
    // ソケットをクローズ

    ycloseSocket();


    // ※  ycloseSocket() を実行すると initialize() 前の初期状態になってしまう
    //    ため，YLIB_DEST_UNINITIALIZE フラグを設定する。
    //    このフラグは initialize() は実行されたが，ソケットが未セットアップ
    //    という状態。
    //    もしも，YLIB_DEST_UNINITILAIZE を設定せずセットアップすると，
    //    環境変数から送信先情報を取得する。
    //      →  下で指定した送信先情報が無効になる。

    _yset_s_dest( YLIB_DEST_UNINITIALIZE );


    //--------------------------------------------------------------------------
    // 送信先文字列をセットする

    _yset_str_dest( str );

    if( flag_tracemode ) {
        syslog( LOG_DEBUG, "ysetDestination() [%lu] [%s].",
                pthread_self(), str );
    }


    //--------------------------------------------------------------------------
    // ソケットを再セットアップ

    int ret;
    ret = ysetupSocket();

    return( ret );
}



//==============================================================================
// 送信先文字列取得
// char* ygetDestination()
//    引数: なし
//    返値: char* 送信先文字列へのポインタ
//             ( [通信方式]://[送信先IPアドレス(IPv4)]:[ポート番号] )
//
// <ver.1.05 で追加 suzuki@seamark.co.jp>

char* YLIB_API ygetDestination()
{
    //--------------------------------------------------------------------------
    // 初期化

    _yinitialize();

    ysendTimeStampMetric("Seamark|ylibcore:alive");
    return ( _yget_str_dest() );
}



//==============================================================================
// 以下，ライブラリ内部で使用する関数群
//==============================================================================


//==============================================================================
// 自スレッドの 変数初期化
// int _yinitialize()
//    引数: なし
//    返値: なし
//    <ver.1.04 で追加 suzuki@seamark.co.jp>
//

void _yinitialize()
{

    //--------------------------------------------------------------------------
    // TSD領域確保 (又は領域へのポインタを取得)

    tsd_buffer *tc ;
    tc = _yget_tsd_buffer();

    // ディスクリプタ s_dest が既に初期化・代入されていたら
    // 既にこの関数が呼び出されているので用無し
#ifdef WIN32
	if( tc != NULL && tc->str_local != NULL && strlen(tc->str_local)>0) return;
#else
    if( tc == NULL || tc->s_dest != 0 ) {
        return ;
    }
    tc->s_dest = YLIB_DEST_UNINITIALIZE;
#endif

    // ※  ポインタ tc はここまで。以降では使用しない。
    //    TSD 領域へのアクセスはそれぞれの関数を用いてアクセスする。

    
    // ホスト名を取得してTSD領域に保存
    // <2007.10.22に追加>
    //_yset_str_local();
#ifdef WIN32
	WSAData wsaData;
	WSAStartup(MAKEWORD(2,0), &wsaData);
	_yset_str_local();
#endif

    //--------------------------------------------------------------------------
    // システム記録プログラム(syslog)への接続開始
#ifndef WIN32
    openlog( "ylib", LOG_CONS | LOG_PID, LOG_USER );
#endif
    // setlogmask( LOG_UPTO( LOG_INFO ) );
    // setlogmask( LOG_DEBUG );


    // char  buff[256];
    // syslog( LOG_DEBUG, "in initialize() %s ", _yget_str_utctime( buff )  );

    //--------------------------------------------------------------------------
    // グローバル変数の初期値 

    // 「送信最大文字列長」の初期値
    max_msglen = YSEND_MSGMAXLEN;

    // 「トレース出力」の初期値，デフォルト: OFF に
    flag_tracemode = YLIB_FALSE;

    // 「送信すべきフラグ」の初期値，デフォルト: ON(送信)に
    sendstatus = YLIB_TRUE;

    // UTC-time の精度 「ミリ」秒に
    time_resolution = YLIB_RESOLUTION_MILI;


    //--------------------------------------------------------------------------
    // 初期値設定ファイル(INIFILENAME) から値を取得
    //

    FILE* fp;
    char  buf  [BUFSIZ];
    char  label[BUFSIZ];
    char  value[BUFSIZ];

    if( ( fp = fopen( INIFILENAME, "r" ) ) != NULL ) {

        while( fgets( buf, BUFSIZ, fp )) {
            sscanf( buf, "%s %s", label, value );
            // printf( "<%s>,<%s>\n", label, value );

            // 送信最大文字列長
            if( strcmp( label, "YSEND_MSGMAXLEN" ) == 0 ) {

                max_msglen = atoi( value );
                // printf( "set max_msglen by file\n" );

            // 送信先
            } else if( strcmp( label, "YSEND_DESTINATION" ) == 0 ) {

                // printf( "<%s>,<%s>\n", label, value );

                // 送信先文字列格納
                _yset_str_dest( value );

            // トレース出力
            } else if( strcmp( label, "YSEND_TRACE" ) == 0 ) {

                if( strcmp( value, "ON" ) == 0 ) { flag_tracemode = YLIB_TRUE; }
                else                             { flag_tracemode = YLIB_FALSE; }

            // 送信すべきフラグ (sendStatus)
            } else if( strcmp( label, "YSEND_SENDSTATUS" ) == 0 ) {

                if( strcmp( value, "ON" ) == 0 ) { sendstatus = YLIB_TRUE;    }
                else                             { sendstatus = YLIB_FALSE; }

            // UTC-time の精度 ( time_resolution: UTC-timeの精度)の値
            //    <ver.1.03 で追加 suzuki@seamark.co.jp>
            } else if( strcmp( label, "YSEND_RESOLUTION" ) == 0 ) {

                if( strcmp( value, "MICRO" ) == 0 ) {
                    time_resolution = YLIB_RESOLUTION_MICRO;
                } else {
                    time_resolution = YLIB_RESOLUTION_MILI;
                }
            }
        }

        fclose( fp );
    }


    //--------------------------------------------------------------------------
    // 環境変数から
    //

    char* pvalue;

    // 送信最大文字列長
    pvalue = getenv( "YSEND_MSGMAXLEN" );
    if( pvalue ) {
        max_msglen = atoi( pvalue );
        //printf( "set max_msglen by env\n" );
    }
        
    // 送信先
    pvalue = getenv( "YSEND_DESTINATION" );
    if( pvalue ) {

        // 送信先文字列格納
        _yset_str_dest( pvalue );
    }

    // トレース出力
    pvalue = getenv( "YSEND_TRACE" );
    if( pvalue ) {
        if( strcmp( pvalue, "ON" ) == 0 ) { flag_tracemode = YLIB_TRUE; }
        else                              { flag_tracemode = YLIB_FALSE; }
    }

    // 送信すべきフラグ (sendStatus)
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    pvalue = getenv( "YSEND_SENDSTATUS" );
    if( pvalue ) {
        if( strcmp( pvalue, "ON" ) == 0 ) { sendstatus = YLIB_TRUE;    }
        else                              { sendstatus = YLIB_FALSE; }
    }

    // UTC-time の精度 ( time_resolution: UTC-timeの精度)の値
    //    <ver.1.03 で追加 suzuki@seamark.co.jp>
    pvalue = getenv( "YSEND_RESOLUTION" );
    if( pvalue ) {

        if( strcmp( pvalue, "MICRO" ) == 0 ) {
            time_resolution = YLIB_RESOLUTION_MICRO;
        } else {
            time_resolution = YLIB_RESOLUTION_MILI;
        }
    }
    return ;
}




//==============================================================================
// 自スレッドの seqNo を設定
// void _yreset_seqNo( long val )
//    引数: val 設定値
//    返値: なし
//

void _yreset_seqNo( long val )
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();

    if ( tc != NULL )
    	tc->seqNo = val ;
}



//==============================================================================
// 自スレッドの seqNo 値に 1 を加算
// long _ycountup_seqNo()
//    引数: なし
//    返値: long seqNo 値 (加算処理後の値)
//

long _ycountup_seqNo()
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();
    if ( tc != NULL ){
        tc->seqNo ++ ;
        return( tc->seqNo );
    }else{
	return(1);
   }
}



//==============================================================================
// 自スレッドの seqNo 値を取得
// int _yget_seqNo()
//    引数: なし
//    返値: int seqNo 値
//

long _yget_seqNo()
{
    tsd_buffer *tc ;
    tc = _yget_tsd_buffer();

    if ( tc != NULL )
    	return( tc->seqNo );
    else
	return( 1 );
}



//==============================================================================
// 自スレッドの 送信先ディスクリプタを設定
// int _yset_s_dest( int val )
//    引数: val ディスクリプタ
//    返値: int ディスクリプタ(引数と同値)
//

SOCKET _yset_s_dest( SOCKET val )
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();
    if ( tc != NULL )
    	tc->s_dest = val ;

    return( val );
}


//==============================================================================
// 自スレッドの 送信先ディスクリプタを取得
// int _yget_s_dest()
//    引数: なし
//    返値: int ディスクリプタ
//

SOCKET _yget_s_dest()
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();

    if ( tc != NULL )
    	return( tc->s_dest );
    else
	return (YLIB_DEST_UNINITIALIZE);
}


//==============================================================================
// 自スレッドの 通信方式を設定
// int _yset_socket_type( int val )
//    引数: val 通信方式
//    返値: int 通信方式(引数と同値)
//    <ver.1.04 で追加 suzuki@seamark.co.jp>
//

int _yset_socket_type( int val )
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();
    if ( tc != NULL)
    	tc->socket_type = val ;

    return( val );
}


//==============================================================================
// 自スレッドの 通信方式を取得
// int _yget_socket_type()
//    引数: なし
//    返値: int 通信方式
//    <ver.1.04 で追加 suzuki@seamark.co.jp>
//

int _yget_socket_type()
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();
    if ( tc != NULL)
    	return( tc->socket_type );
    else
	return(SOCK_DGRAM);
}



//==============================================================================
// 自スレッドの 送信先(YSEND_DESTINATION指定値)文字列を設定
// void _yset_str_dest( char* str )
//    引数: str 送信先文字列へのポインタ
//    返値: なし
//

void _yset_str_dest( char* str )
{
    if( str == NULL ) {
        return;
    }

    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();
    if ( tc != NULL){
    	strcpy( tc->str_dest, str );
    }
}


//==============================================================================
// 自スレッドの 送信先(YSEND_DESTINATION指定値)文字列を取得
// char* _yget_str_dest()
//    引数: なし
//    返値: char* 送信先文字列
//

char* _yget_str_dest()
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();

    if( tc != NULL && tc->str_dest )
		 { return( tc->str_dest ); }
    else         { return( "" );           }
}


//==============================================================================
// UTC-time (エポックからの経過時間の)文字列を生成
// char* _yget_str_utctime()
//    引数: char* buff 経過時間文字列格納領域(文字配列)へのポインタ
//          ※ 17文字以上の領域を確保してください。
//             領域長のチェックはしていません。
//    返値: char* 生成した経過時間文字列(へのポインタ)
//    <ver.1.03 で追加 suzuki@seamark.co.jp>
//

//char* _yget_str_utctime( char* buff )
unsigned long _yget_long_utctime()
{
#ifndef WIN32
    // 時刻を取得する
    struct timeval  tv;
    gettimeofday( &tv, NULL );
    unsigned long ret;

    if( time_resolution == YLIB_RESOLUTION_MICRO ) {
        ret = tv.tv_sec*1000000 + tv.tv_usec;
    } else {
        ret = tv.tv_sec*1000000 + tv.tv_usec/1000;
    }
    return ret;
#else
	SYSTEMTIME	time_val;
	GetLocalTime( &time_val );
	time_t t = time(NULL);
	unsigned long ret;
	//if( time_resolution == YLIB_RESOLUTION_MICRO ) 
		ret = (long)t*1000000L + time_val.wMilliseconds;
	//else
	//	ret = t*1000000L + time_val.wMilliseconds;
    return ret;
#endif

}
void  _yget_str_utctime( char* buff )
{
#ifndef WIN32
    // 時刻を取得する
    struct timeval  tv;
    gettimeofday( &tv, NULL );

    if( time_resolution == YLIB_RESOLUTION_MICRO ) {
        snprintf(buff,UTCTIME_LEN-1,"%lu%06lu",tv.tv_sec,tv.tv_usec);
        //snprintf(buff,10,"%u",tv.tv_sec);
        //snprintf(buff+10,6,"%06u",tv.tv_usec);
    } else {
        snprintf(buff,UTCTIME_LEN-1,"%lu%03lu",tv.tv_sec,(tv.tv_usec/1000));
        //snprintf(buff,10,"%u",tv.tv_sec);
        //snprintf(buff+10,3,"%u",(tv.tv_usec/1000));
    }
#else
	SYSTEMTIME	time_val;
	GetLocalTime( &time_val );
	time_t t = time(NULL);
	int j = snprintf(buff,UTCTIME_LEN-1,"%lu",t);
	if( time_resolution == YLIB_RESOLUTION_MICRO ){
		 j+= sprintf(buff + j, "%06u",time_val.wMilliseconds );
	}else{
		 j+= sprintf(buff + j, "%03u",time_val.wMilliseconds );
	}

#endif

}



//==============================================================================
// 自スレッドの 送信先情報(sockaddr_in構造体:IPアドレスとポート番号)を設定
// void _yset_saddr( struct sockaddr_in* s )
//    引数: sockaddr_in 構造体へのポインタ
//    返値: なし
//

void _yset_saddr( struct sockaddr_in* s )
{
    if( s == NULL ) {
        return;
    }

    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();

    if ( tc != NULL){
        bzero( (char *)&(tc->saddr), sizeof( struct sockaddr_in ) );

        tc->saddr.sin_addr   = s->sin_addr;
        tc->saddr.sin_family = s->sin_family;
        tc->saddr.sin_port   = s->sin_port;
    }

}


//==============================================================================
// 自ホスト名をTSD領域に設定
// void _yset_str_local()
//    引数: なし
//    返値: なし
//
// <2007.10.22に追加>
void _yset_str_local()
{
    tsd_buffer *tc ;
    // TSDへのポインタを取得
    tc = _yget_tsd_buffer();
    if ( tc != NULL){
#ifndef WIN32
		// ホスト名を取得する
        char str_hostname [HOSTNAME_LEN];
        memset( str_hostname, 0, HOSTNAME_LEN );

        gethostname( str_hostname, HOSTNAME_LEN-1 );
#else
		// コンピュータ名取得
		char str_hostname [HOSTNAME_LEN];
        memset( str_hostname, 0, HOSTNAME_LEN );
		if (gethostname(str_hostname,sizeof(str_hostname)-1) ) {
			strcpy(str_hostname,"localhost");
		}
#endif
        //取得したホスト名をTSDに設定する
        strcpy( tc->str_local, str_hostname );
    }
}


//==============================================================================
// 自ホスト名をTSD領域から取得
// char* _yget_str_local()
//    引数: なし
//    返値: str_localへのポインタ
//
// <2007.10.22に追加>
char*  _yget_str_local()
{
    tsd_buffer *tc ;

    // TSDへのポインタを取得
    tc = _yget_tsd_buffer();

    if( tc != NULL && tc->str_local )
		 { return( tc->str_local ); }
    else         { return( "" );           }
}


//==============================================================================
// 自スレッドの 送信先情報(sockaddr_in構造体:IPアドレスとポート番号)を取得
// struct sockaddr_in* _yget_saddr()
//    引数: なし
//    返値: sockaddr_in 構造体へのポインタ
//

struct sockaddr_in* _yget_saddr()
{
    tsd_buffer *tc ;

    tc = _yget_tsd_buffer();

    if ( tc != NULL)
    	return( &(tc->saddr) );
    else
	return(NULL);
}



#ifndef WIN32
//==============================================================================
// 自スレッド固有データ(TSD)領域へのキーを確保
// void _ytsd_buffer_key_init()
//    引数: なし
//    返値: なし
//

void _ytsd_buffer_key_init()
{
    pthread_key_create( &tsd_buffer_key,_destroy_tsd );
}

#endif

static void _destroy_tsd(void * tsd)
{

	if ( tsd != NULL ){
	   if( flag_tracemode ) {
	        syslog( LOG_DEBUG, "finalize [%lu] destination [%s]",
	                pthread_self(), 
			strcmp(_yget_str_dest(),"")==0?"closed":_yget_str_dest() );
	   }
#ifdef WIN32
	   TlsFree(tsd_buffer_key);
#endif
	   ycloseSocket();
	   YFREE(tsd);
	}

}

#ifdef __NOT_USE_TLS__
static tsd_buffer* _getAccess(){
	if ( gAccess == NULL ){
          gAccess = (tsd_buffer*)YMALLOC( sizeof(tsd_buffer) );
          memset( gAccess, 0, sizeof( tsd_buffer ) );
		  gAccess->tid = ygetGUID();
	}
	return gAccess;
	
}
#endif
//==============================================================================
// 自スレッド固有データ(TSD)領域へのポインタを取得
// tsd_buffer* _yget_tsd_buffer()
//    引数: なし
//    返値: tsd_buffer* TSD領域へのポインタ
//

tsd_buffer* _yget_tsd_buffer()
{
   tsd_buffer *tc ;
#ifdef __NOT_USE_TLS__
   tc = _getAccess() ;
   return( tc );
#else

#ifdef WIN32
   	tc = (tsd_buffer*)TlsGetValue(tsd_buffer_key);
	if ( tc == NULL ){
		tc = (tsd_buffer*)YMALLOC(sizeof(tsd_buffer));
        memset( tc, 0, sizeof( tsd_buffer ) );
		tc->tid = (DWORD)tc;//ygetGUID();	    
		if ( TlsSetValue(tsd_buffer_key,tc) == NULL ){
			syslog( LOG_ERR, "_yget_tsd_buffer() [%lu] malloc error. no memory for tsd_buffer.",
                pthread_self() );
			return (tsd_buffer*)NULL;
		}
	}
#else
    tc = pthread_getspecific( tsd_buffer_key );

    if( tc == 0 ) {

        pthread_once( &tsd_buffer_alloc_once, _ytsd_buffer_key_init );

        tc = (tsd_buffer*)YMALLOC( sizeof(tsd_buffer));

        if( tc == 0 ) {

            syslog( LOG_ERR, 
                "_yget_tsd_buffer() [%lu] malloc error. no memory for tsd_buffer.",
                pthread_self() );

            return( (tsd_buffer*)NULL );
        }

        memset( tc, 0, sizeof( tsd_buffer ) );

        if( pthread_setspecific( tsd_buffer_key, tc ) != 0 ) {

            syslog( LOG_ERR, "_yget_tsd_buffer() [%lu] error pthread_setspecific()", 
                    pthread_self() );

            return( (tsd_buffer*)NULL );
        }
    }
#endif
    return( tc );
#endif
}



//==============================================================================
// 文字列置換( '\t', '\n', '\r' を削除 )
// void _yreplace_str( char* str )
//    引数: 文字列
//    返値: なし
//    <ver.1.06 で追加 suzuki@seamark.co.jp>
/*
void _yreplace_str( char* str )
{
    if( str == NULL ) {
        return;
    }

    char*  p_str;

    for( p_str = str ; *p_str != '\0' ; p_str++ ) {

        switch( (int)*p_str ) {
        case '\t':
        case '\n':
        case '\r':
            // 空白文字に置換する場合 (下2行を有効に)
            *p_str = ' ';
//            p_str++;
            break;
        default:
            break;
        }
    }
}
*/

void _yreplace_str( char* str )
{
//    return;
    if( str == NULL ) {
        return;
    }

    char*  buf;
    char*  p_buf;
    char*  p_str;

    buf = (char*)YCALLOC( strlen( str ) + 1, sizeof( char ) );

    if( buf == NULL ) {
        return;
    }

    for( p_str = str, p_buf = buf ; *p_str != '\0' ; p_str++ ) {

        switch( (int)*p_str ) {
        case '\t':
        case '\n':
        case '\r':
            // 空白文字に置換する場合 (下2行を有効に)
            *p_buf = ' ';
            p_buf++;
            break;

        default:
            *p_buf = *p_str;
            p_buf++;
            *p_buf = '\0';
            break;
        }
    }
    //trailing space have to delete.
    int i = strlen(buf);
    for( ; i>=0; i--){
	if ( buf[i]==' ' || buf[i] =='\0' )
	   buf[i]='\0';
	else
	   break;
    }
    strcpy( str, buf );
    YFREE( buf );
}

