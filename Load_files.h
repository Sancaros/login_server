#
//载入允许的任务物品文件
void LoadQuestAllow()
{
	unsigned ch;
	char allow_data[256];
	unsigned char* qa;
	FILE* fp;

	quest_numallows = 0;
	fopen_s(&fp, "questitem.txt", "r");
	if (fp == NULL)
	{
		printf("questitem.txt 文件缺失.\n");
		printf("按下 [回车键] 退出...");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	else
	{
		while (fgets(&allow_data[0], 255, fp) != NULL)
		{
			if ((allow_data[0] != 35) && (strlen(&allow_data[0]) > 5))
				quest_numallows++;
		}
		quest_allow = malloc(quest_numallows * 4);
		ch = 0;
		fseek(fp, 0, SEEK_SET);
		while (fgets(&allow_data[0], 255, fp) != NULL)
		{
			if ((allow_data[0] != 35) && (strlen(&allow_data[0]) > 5))
			{
				quest_allow[ch] = 0;
				qa = (unsigned char*)&quest_allow[ch++];
				qa[0] = hexToByte(&allow_data[0]);
				qa[1] = hexToByte(&allow_data[2]);
				qa[2] = hexToByte(&allow_data[4]);
			}
		}
		fclose(fp);
	}
	printf("任务物品奖励数量: %u\n", quest_numallows);
}

//载入掉落数据
void LoadDropData()
{
	unsigned ch, ch2, ch3, d;
	unsigned char* rt_table;
	char id_file[256];
	FILE* fp;
	char convert_ch[10];
	int look_rate;

	printf("正在载入掉落数据...\n");

	// Each episode 每种章节
	for (ch = 1;ch < 5;ch++)
	{
		if (ch != 3)
		{
			switch (ch)
			{
			case 0x01:
				rt_table = (unsigned char*)&rt_tables_ep1[0];
				break;
			case 0x02:
				rt_table = (unsigned char*)&rt_tables_ep2[0];
				break;
			case 0x04:
				rt_table = (unsigned char*)&rt_tables_ep4[0];
				break;
			}
			// Each difficulty 每种难度
			for (d = 0;d < 4;d++)
			{
				// Each section ID 每种颜色ID
				for (ch2 = 0;ch2 < 10;ch2++)
				{
					id_file[0] = 0;
					switch (ch)
					{
					case 0x01:
						strcat(&id_file[0], "drop\\ep1_mob_");
						break;
					case 0x02:
						strcat(&id_file[0], "drop\\ep2_mob_");
						break;
					case 0x04:
						strcat(&id_file[0], "drop\\ep4_mob_");
						break;
					}
					_itoa(d, &convert_ch[0], 10);
					strcat(&id_file[0], &convert_ch[0]);
					strcat(&id_file[0], "_");
					_itoa(ch2, &convert_ch[0], 10);
					strcat(&id_file[0], &convert_ch[0]);
					strcat(&id_file[0], ".txt");
					ch3 = 0;
					fp = fopen(&id_file[0], "r");
					if (!fp)
					{
						printf("掉落表未找到 \"%s\"", (char*)id_file[0]);
						printf("按下 [回车键] 退出...");
						gets_s(&dp[0], 0);
						exit(1);
					}
					look_rate = 1;
					while (fgets(&dp[0], 255, fp) != NULL)
					{
						if (dp[0] != 35) // not a comment
						{
							if (look_rate)
							{
								rt_table[ch3++] = (unsigned char)atoi(&dp[0]);
								look_rate = 0;
							}
							else
							{
								if (strlen(&dp[0]) < 6)
								{
									printf("掉落表已损坏 \"%s\"", (char*)id_file[0]);
									printf("按下 [回车键] 退出...");
									gets_s(&dp[0], 0);
									exit(1);
								}
								_strupr(&dp[0]);
								rt_table[ch3++] = hexToByte(&dp[0]);
								rt_table[ch3++] = hexToByte(&dp[2]);
								rt_table[ch3++] = hexToByte(&dp[4]);
								look_rate = 1;
							}
						}
					}
					fclose(fp);
					ch3 = 0x194;
					memset(&rt_table[ch3], 0xFF, 30);
					id_file[9] = 98;
					id_file[10] = 111;
					id_file[11] = 120;
					fp = fopen(&id_file[0], "r");
					if (!fp)
					{
						printf("掉落表未找到 \"%s\"", (char*)id_file[0]);
						printf("按下 [回车键] 退出...");
						gets_s(&dp[0], 0);
						exit(1);
					}
					look_rate = 0;
					while ((fgets(&dp[0], 255, fp) != NULL) && (ch3 < 0x1B2))
					{
						if (dp[0] != 35) // not a comment
						{
							switch (look_rate)
							{
							case 0x00:
								rt_table[ch3] = (unsigned char)atoi(&dp[0]);
								look_rate = 1;
								break;
							case 0x01:
								rt_table[0x1B2 + ((ch3 - 0x194) * 4)] = (unsigned char)atoi(&dp[0]);
								look_rate = 2;
								break;
							case 0x02:
								if (strlen(&dp[0]) < 6)
								{
									printf("掉落表已损坏 \"%s\"", (char*)id_file[0]);
									printf("按下 [回车键] 退出...");
									gets_s(&dp[0], 0);
									exit(1);
								}
								_strupr(&dp[0]);
								rt_table[0x1B3 + ((ch3 - 0x194) * 4)] = hexToByte(&dp[0]);
								rt_table[0x1B4 + ((ch3 - 0x194) * 4)] = hexToByte(&dp[2]);
								rt_table[0x1B5 + ((ch3 - 0x194) * 4)] = hexToByte(&dp[4]);
								look_rate = 0;
								ch3++;
								break;
							}
						}
					}
					fclose(fp);
					rt_table += 0x800;
				}
			}
		}
	}
}