/*  commands.c
 *
 *
 *  Copyright (C) 2014 toxbot All Rights Reserved.
 *
 *  This file is part of toxbot.
 *
 *  toxbot is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  toxbot is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with toxbot. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include "toxbot.h"
#include "misc.h"
#include "groupchats.h"

#define MAX_COMMAND_LENGTH TOX_MAX_MESSAGE_LENGTH
#define MAX_NUM_ARGS 4

extern char *DATA_FILE;
extern char *MASTERLIST_FILE;
extern struct Tox_Bot Tox_Bot;

static void authent_failed(Tox *m, uint32_t friendnum)
{
    const char *outmsg = "您无权使用此命令。";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void send_error(Tox *m, uint32_t friendnum, const char *message, int err)
{
    char outmsg[TOX_MAX_MESSAGE_LENGTH];
    snprintf(outmsg, sizeof(outmsg), "%s (error %d)", message, err);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_default(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "错误：需要房间号码";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if ((groupnum == 0 && strcmp(argv[1], "0")) || groupnum < 0) {
        outmsg = "错误：需要房间号码";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    Tox_Bot.default_groupnum = groupnum;

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "默认房间号设置为 %d", groupnum);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    printf("默认房间号设置为 %d by %s", groupnum, name);
}

static void cmd_gmessage(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "错误：需要群编号";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argc < 2) {
        outmsg = "错误：需要消息";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "错误：需要群编号";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (group_index(groupnum) == -1) {
        outmsg = "错误：需要群编号";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "错误：消息必须用引号括起来";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[2][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    TOX_ERR_CONFERENCE_SEND_MESSAGE err;

    if (!tox_conference_send_message(m, groupnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), &err)) {
        outmsg = "错误：无法发送消息。";
        send_error(m, friendnum, outmsg, err);
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    outmsg = "消息发送.";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    printf("<%s> 消息到群 %d: %s\n", name, groupnum, msg);
}

static void cmd_group(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (argc < 1) {
        outmsg = "请指定组类型: audio 或 text";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    uint8_t type = TOX_CONFERENCE_TYPE_AV ? !strcasecmp(argv[1], "audio") : TOX_CONFERENCE_TYPE_TEXT;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_CONFERENCE_TYPE_TEXT) {
        TOX_ERR_CONFERENCE_NEW err;
        groupnum = tox_conference_new(m, &err);

        if (err != TOX_ERR_CONFERENCE_NEW_OK) {
            printf("创建群聊 %s 初始化失败\n", name);
            outmsg = "群聊实例无法初始化。";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
            return;
        }
    } else if (type == TOX_CONFERENCE_TYPE_AV) {
        groupnum = toxav_add_av_groupchat(m, NULL, NULL);

        if (groupnum == -1) {
            printf("创建群聊 %s 初始化失败\n", name);
            outmsg = "群聊实例无法初始化。";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
            return;
        }
    }

    const char *password = argc >= 2 ? argv[2] : NULL;

    if (password && strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        printf("创建群聊 %s 失败: 密码太长\n", name);
        outmsg = "创建群聊失败，密码太长";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (group_add(groupnum, type, password) == -1) {
        printf("创建群聊 %s 失败\n", name);
        outmsg = "创建群聊失败";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        tox_conference_delete(m, groupnum, NULL);
        return;
    }

    const char *pw = password ? " ( 密码保护 )" : "";
    printf("群聊 %d 创建成功 %s%s\n", groupnum, name, pw);

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "群聊 %d 创建%s", groupnum, pw);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);
}

static void cmd_help(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    outmsg = "info : 反馈当前状态并列出活跃群聊";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "id : 反馈当前机器人ID";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "invite : 加入默认群聊";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "invite <n> <p> : 请求加入群聊天，n为群聊ID，p为密码(如果有密码)";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "group <type> <pass> : 创建一个群聊，type为类型，默认为文本text，可以选择“audio”带语音功能，pass为密码。";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    if (friend_is_master(m, friendnum)) {
        outmsg = "管理员命令默认不允许执行";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    }
}

static void cmd_id(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[TOX_ADDRESS_SIZE * 2 + 1];
    char address[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        char d[3];
        sprintf(d, "%02X", address[i] & 0xff);
        memcpy(outmsg + i * 2, d, 2);
    }

    outmsg[TOX_ADDRESS_SIZE * 2] = '\0';
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_info(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[MAX_COMMAND_LENGTH];
    char timestr[64];

    uint64_t curtime = (uint64_t) time(NULL);
    get_elapsed_time_str(timestr, sizeof(timestr), curtime - Tox_Bot.start_time);
    snprintf(outmsg, sizeof(outmsg), "启动时间: %s", timestr);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    uint32_t numfriends = tox_self_get_friend_list_size(m);
    snprintf(outmsg, sizeof(outmsg), "好友数量: %d (%d online)", numfriends, Tox_Bot.num_online_friends);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    snprintf(outmsg, sizeof(outmsg), "不活跃好友清除 %"PRIu64" 天",
             Tox_Bot.inactive_limit / SECONDS_IN_DAY);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    /* List active group chats and number of peers in each */
    size_t num_chats = tox_conference_get_chatlist_size(m);

    if (num_chats == 0) {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) "机器人没有群聊",
                                strlen("机器人没有群聊"), NULL);
        return;
    }

    uint32_t groupchat_list[num_chats];

    tox_conference_get_chatlist(m, groupchat_list);

    uint32_t i;

    for (i = 0; i < num_chats; ++i) {
        TOX_ERR_CONFERENCE_PEER_QUERY err;
        uint32_t groupnum = groupchat_list[i];
        uint32_t num_peers = tox_conference_peer_count(m, groupnum, &err);

        if (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
            int idx = group_index(groupnum);
            const char *title = Tox_Bot.g_chats[idx].title_len
                                ? Tox_Bot.g_chats[idx].title : "未设置群名称";
            const char *type = tox_conference_get_type(m, groupnum, NULL) == TOX_CONFERENCE_TYPE_AV ? "Audio" : "Text";
            snprintf(outmsg, sizeof(outmsg), "群ID： %d | %s | 在线人数: %d | 群名称: %s", groupnum, type,
                     num_peers, title);
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        }
    }
}

static void cmd_invite(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;
    int groupnum = Tox_Bot.default_groupnum;

    if (argc >= 1) {
        groupnum = atoi(argv[1]);

        if (groupnum == 0 && strcmp(argv[1], "0")) {
            outmsg = "错误：群ID无效，请重新输入";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
            return;
        }
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "这个群不存在";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int has_pass = Tox_Bot.g_chats[idx].has_pass;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    const char *passwd = NULL;

    if (argc >= 2) {
        passwd = argv[2];
    }

    if (has_pass && (!passwd || strcmp(argv[2], Tox_Bot.g_chats[idx].password) != 0)) {
        fprintf(stderr, "无法邀请 %s 到群 %d (密码错误)\n", name, groupnum);
        outmsg = "密码错误";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    TOX_ERR_CONFERENCE_INVITE err;

    if (!tox_conference_invite(m, friendnum, groupnum, &err)) {
        fprintf(stderr, "无法邀请 %s 到群 %d\n", name, groupnum);
        outmsg = "邀请失败";
        send_error(m, friendnum, outmsg, err);
        return;
    }

    printf("邀请 %s 到群 %d\n", name, groupnum);
}

static void cmd_leave(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "错误：需要群ID";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "错误：群ID无效";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (!tox_conference_delete(m, groupnum, NULL)) {
        outmsg = "错误：群ID无效";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    char msg[MAX_COMMAND_LENGTH];

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    group_leave(groupnum);

    printf("退出群 %d (%s)\n", groupnum, name);
    snprintf(msg, sizeof(msg), "退出群 %d", groupnum);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);
}

static void cmd_master(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "错误：需要Tox ID";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    const char *id = argv[1];

    if (strlen(id) != TOX_ADDRESS_SIZE * 2) {
        outmsg = "错误：需要Tox ID";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "a");

    if (fp == NULL) {
        outmsg = "错误：找不到masterkeys文件";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    fprintf(fp, "%s\n", id);
    fclose(fp);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    printf("%s 添加管理员: %s\n", name, id);
    outmsg = "ID已添加到管理员列表中";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_name(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "错误：需要名称";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int len = 0;

    if (argv[1][0] == '\"') {    /* remove opening and closing quotes */
        snprintf(name, sizeof(name), "%s", &argv[1][1]);
        len = strlen(name) - 1;
    } else {
        snprintf(name, sizeof(name), "%s", argv[1]);
        len = strlen(name);
    }

    name[len] = '\0';
    tox_self_set_name(m, (uint8_t *) name, (uint16_t) len, NULL);

    char m_name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) m_name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    m_name[nlen] = '\0';

    printf("%s 设置名字 %s\n", m_name, name);
    save_data(m, DATA_FILE);
}

static void cmd_passwd(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "错误：群ID是必须输入的";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "错误：群ID无效";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "错误：群ID无效";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';


    /* no password */
    if (argc < 2) {
        Tox_Bot.g_chats[idx].has_pass = false;
        memset(Tox_Bot.g_chats[idx].password, 0, MAX_PASSWORD_SIZE);

        outmsg = "没有设置密码";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        printf("没有为群聊设置密码 %d by %s\n", groupnum, name);
        return;
    }

    if (strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        outmsg = "密码太长";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    Tox_Bot.g_chats[idx].has_pass = true;
    snprintf(Tox_Bot.g_chats[idx].password, sizeof(Tox_Bot.g_chats[idx].password), "%s", argv[2]);

    outmsg = "设置密码";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    printf("群聊 %d 密码设置 %s\n", groupnum, name);

}

static void cmd_purge(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: number > 0 required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    uint64_t days = (uint64_t) atoi(argv[1]);

    if (days <= 0) {
        outmsg = "Error: number > 0 required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    uint64_t seconds = days * SECONDS_IN_DAY;
    Tox_Bot.inactive_limit = seconds;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Purge time set to %"PRIu64" days", days);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);

    printf("Purge time set to %"PRIu64" days by %s\n", days, name);
}

static void cmd_status(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: status required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    TOX_USER_STATUS type;
    const char *status = argv[1];

    if (strcasecmp(status, "online") == 0) {
        type = TOX_USER_STATUS_NONE;
    } else if (strcasecmp(status, "away") == 0) {
        type = TOX_USER_STATUS_AWAY;
    } else if (strcasecmp(status, "busy") == 0) {
        type = TOX_USER_STATUS_BUSY;
    } else {
        outmsg = "Invalid status. Valid statuses are: online, busy and away.";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    tox_self_set_status(m, type);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    printf("%s set status to %s\n", name, status);
    save_data(m, DATA_FILE);
}

static void cmd_statusmessage(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: message required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argv[1][0] != '\"') {
        outmsg = "错误：消息必须用引号括起来";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[1][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    tox_self_set_status_message(m, (uint8_t *) msg, len, NULL);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    printf("%s set status message to \"%s\"\n", name, msg);
    save_data(m, DATA_FILE);
}

static void cmd_title_set(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 2) {
        outmsg = "Error: Two arguments are required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Error: title must be enclosed in quotes";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    /* remove opening and closing quotes */
    char title[MAX_COMMAND_LENGTH];
    snprintf(title, sizeof(title), "%s", &argv[2][1]);
    int len = strlen(title) - 1;
    title[len] = '\0';

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    TOX_ERR_CONFERENCE_TITLE err;

    if (!tox_conference_set_title(m, groupnum, (uint8_t *) title, len, &err)) {
        printf("%s failed to set the title '%s' for group %d\n", name, title, groupnum);
        outmsg = "Failed to set title. This may be caused by an invalid group number or an empty room";
        send_error(m, friendnum, outmsg, err);
        return;
    }

    int idx = group_index(groupnum);
    memcpy(Tox_Bot.g_chats[idx].title, title, len + 1);
    Tox_Bot.g_chats[idx].title_len = len;

    outmsg = "Group title set";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    printf("%s set group %d title to %s\n", name, groupnum, title);
}

/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(const char *input, char (*args)[MAX_COMMAND_LENGTH])
{
    char *cmd = strdup(input);

    if (cmd == NULL) {
        exit(EXIT_FAILURE);
    }

    int num_args = 0;
    int i = 0;    /* index of last char in an argument */

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int qt_ofst = 0;    /* set to 1 to offset index for quote char at end of arg */

        if (*cmd == '\"') {
            qt_ofst = 1;
            i = char_find(1, cmd, '\"');

            if (cmd[i] == '\0') {
                free(cmd);
                return -1;
            }
        } else {
            i = char_find(0, cmd, ' ');
        }

        memcpy(args[num_args], cmd, i + qt_ofst);
        args[num_args++][i + qt_ofst] = '\0';

        if (cmd[i] == '\0') {  /* no more args */
            break;
        }

        char tmp[MAX_COMMAND_LENGTH];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    free(cmd);
    return num_args;
}

static struct {
    const char *name;
    void (*func)(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH]);
} commands[] = {
    { "default",          cmd_default       },
    { "group",            cmd_group         },
    { "gmessage",         cmd_gmessage      },
    { "help",             cmd_help          },
    { "id",               cmd_id            },
    { "info",             cmd_info          },
    { "invite",           cmd_invite        },
    { "leave",            cmd_leave         },
    { "master",           cmd_master        },
    { "name",             cmd_name          },
    { "passwd",           cmd_passwd        },
    { "purge",            cmd_purge         },
    { "status",           cmd_status        },
    { "statusmessage",    cmd_statusmessage },
    { "title",            cmd_title_set     },
    { NULL,               NULL              },
};

static int do_command(Tox *m, uint32_t friendnum, int num_args, char (*args)[MAX_COMMAND_LENGTH])
{
    int i;

    for (i = 0; commands[i].name; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(m, friendnum, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

int execute(Tox *m, uint32_t friendnum, const char *input, int length)
{
    if (length >= MAX_COMMAND_LENGTH) {
        return -1;
    }

    char args[MAX_NUM_ARGS][MAX_COMMAND_LENGTH];
    int num_args = parse_command(input, args);

    if (num_args == -1) {
        return -1;
    }

    return do_command(m, friendnum, num_args, args);
}
