#pragma once
#include "../ModText.h"
namespace ReleaseNotice {
inline const char* Body(){return ModText::Tr(
"【MOD114 · 一键匹配】\n"
"· 在线房间新增一键匹配，快速／标准／完整分开组队\n"
"· 自动加入等待中的队伍，无队伍时自动建房\n"
"· 大厅显示匹配中标签，可直接加入\n"
"· 两人起由房主手动开始，最多六席\n"
"· 匹配使用固定规则，首波等待30秒\f"
"【房间与观战】\n"
"· 已开局房间仍可先观战，在安全时点转参战\n"
"· 取消匹配会离开房间，队友可继续招人\n"
"· 保留双端花园五页、全员保龄球与宽屏修订\n"
"· 安卓覆盖安装；电脑保留userdata后更新\n"
"· 请与队友使用相同版本",
"[MOD114 / Find a team]\n"
"· Online rooms now offer one-click matching\n"
"· Quick, Standard and Full runs have separate pools\n"
"· Join a waiting team or create a matching room\n"
"· The host starts with 2+ players, up to 6 seats\n"
"· Fixed rules include a 30-second first wave\f"
"[Rooms and spectators]\n"
"· Watch an ongoing game and join combat at a safe point\n"
"· Cancel matching leaves the room; teammates can stay\n"
"· Includes garden pages, bowling and widescreen updates\n"
"· Update Android in place; keep PC userdata\n"
"· Use the same version as your teammates");}
}
