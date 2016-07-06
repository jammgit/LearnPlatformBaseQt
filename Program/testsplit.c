#include "Apue.h"
#include <stdio.h>
#include <vector>

int main(int argc, char *argv[])
{
	char text[512];
	scanf("%s", text);
	std::vector<std::string> pack = gSplitMsgPack(text);
	for (int i = 0; i < pack.size(); ++i)
	{
		printf("%s\n", pack[i].c_str());
	}
	return 0;
}