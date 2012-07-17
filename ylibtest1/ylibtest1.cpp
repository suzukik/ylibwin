// ylibtest1.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include "..\ylib\ylib.h"

int _tmain(int argc, _TCHAR* argv[])
{
	for(int i=0;i<10000;i++){
	printf("suzuki %d\n",i);
	printf("sent len = %d\n", ysendIntCountMetric("seamark|test:nakamura",2));
	printf("sent len = %d\n", ysendLongCountMetric("seamark|testlong:nakamura","20000"));
	printf("sent len = %d\n", ysendIntAverageMetric("seamark|test:average nakamura",20));
	printf("sent len = %d\n", ysendIntAverageMetric("seamark|test:average nakamura",40));
	printf("sent len = %d\n", ysendLongAverageMetric("seamark|testlong:average nakamura","2000000"));
	printf("sent len = %d\n", ysendLongAverageMetric("seamark|testlong:average nakamura","4000000"));

	Sleep(10);
	}
	return 0;
}

