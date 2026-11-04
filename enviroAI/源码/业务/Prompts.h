#pragma once

#include "Models.h"

#include <QString>

// 根据用户填写的资料，生成一个专属角色的设定（只生成文字/头像，不生成用户姓名）。
inline QString customRolePrompt(const RoleRequest &r)
{
    const QString nameLine = r.name.trimmed().isEmpty()
                                 ? QStringLiteral("用户没有给这个角色填名字，不要在设定里给角色起名。")
                                 : QStringLiteral("用户给这个角色填的名字是“%1”。").arg(r.name.trimmed());
    return QStringLiteral(
        "请为「心屿」生成一个用户自定义的中文角色扮演设定。只输出 JSON 对象，不要输出解释或 Markdown。\n\n"
        "用户资料：用户是%1，%2 岁。\n"
        "角色资料：关系是“%3”；角色性别是%4；角色年龄是%5 岁；性格是“%6”。%7\n\n"
        "JSON 结构：\n"
        "{\"tagline\":\"10-18字的关系定位\",\"avatar\":\"一个最贴切的 emoji\","
        "\"system_prompt\":\"完整的中文角色设定\"}\n\n"
        "要求：\n"
        "1. system_prompt 使用第二人称“你”，写清角色与用户的关系、年龄感、性格、生活背景、"
        "说话习惯、关心方式、边界和禁忌；不要提及 AI、模型、程序或提示词。\n"
        "2. 角色要像一个真实的人：有自己的生活细节、口头习惯和情绪反应；不要每句话都分析、"
        "说教、总结或提问，可以只接一句、吐槽、说自己的事或陪着。\n"
        "3. 关系和年龄要自然、合理，不能和用户资料冲突；父母要有长辈感，老师要有前辈感，"
        "恋人要有亲密感；不要油腻，不要刻板化。\n"
        "4. 称呼随情境自然出现，不要固定成每轮开头；不要用“称呼+先别+建议”的模板。\n"
        "5. avatar 只用一个 emoji。\n"
        "6. 所有文本使用简体中文。若用户给了名字，可以在设定中偶尔使用，但不要反复自称；"
        "若没有名字，绝对不要给角色起名。\n")
        .arg(r.userGender == QStringLiteral("male") ? QStringLiteral("男生") : QStringLiteral("女生"))
        .arg(r.userAge)
        .arg(r.relation.trimmed())
        .arg(r.roleGender == QStringLiteral("male") ? QStringLiteral("男性") : QStringLiteral("女性"))
        .arg(r.roleAge)
        .arg(r.personality.trimmed())
        .arg(nameLine);
}
