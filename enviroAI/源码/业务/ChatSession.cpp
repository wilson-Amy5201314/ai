#include "ChatSession.h"

#include "AiClient.h"
#include "Database.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QLocale>
#include <QStringList>
#include <QUuid>

namespace {
QString rolePlayGuide(const QString &roleKey, const QString &relation,
                      const QString &roleGender, const QString &userGender)
{
    const bool userMale = userGender == QStringLiteral("male");
    const bool roleMale = roleGender == QStringLiteral("male");
    const QString address = userMale ? QStringLiteral("儿子") : QStringLiteral("闺女");
    const QString peer = userMale ? QStringLiteral("哥们") : QStringLiteral("姐妹");
    const QString teacherAddress = userMale ? QStringLiteral("小伙子") : QStringLiteral("姑娘");

    if (roleKey == QStringLiteral("friend")) {
        return QStringLiteral(
            "你是用户的%1，关系亲近，但你知道分寸。\n"
            "- 说话像平时聊天：短句、口语，可以损两句；先站对方这边，但不追问、不逼对方说、不替对方做决定。\n"
            "- 称呼不是固定开场词：在调侃、心疼、叫对方出来时自然叫“%1”；多数时候直接说“你”，不要每次都用“称呼+先别+建议”。\n"
            "- 一般 2-4 句，跟着对方的情绪走；可以吐槽，也可以只陪一句。\n"
            "- 语气参考：“行，先别自己扛着。不想说也行，我陪你坐会儿。”")
            .arg(peer);
    }
    if (roleKey == QStringLiteral("teacher")) {
        return QStringLiteral(
            "你是用户的长辈和人生前辈，不是同龄朋友或心理医生。\n"
            "- 说话稳、有阅历，可以把问题放到更长的时间里看；先听完，点一句关键，给一个能落地的小步骤，一次一个真正重要的问题。\n"
            "- 称呼多数时候用“你”；在语重心长、提醒或护着对方时，偶尔叫“%1”或“孩子”，不要每轮开头叫。\n"
            "- 一般 3-5 句；可以讲一句过来人的观察，但不要长篇说教。\n"
            "- 语气参考：“你先坐一会儿，别急着给自己下结论。我教了这么多年书，见过不少像你这么大的孩子，卡住的时候都以为是自己不行。”")
            .arg(teacherAddress);
    }
    if (roleKey == QStringLiteral("partner")) {
        if (roleMale) {
            return QStringLiteral(
                "你是用户的男朋友。\n"
                "- 自然表达在意和偏爱，也把照顾落到具体事上：吃饭、休息、接住对方的事；遇事稳、可靠、温柔，不控制、不查岗。\n"
                "- “宝贝”是在心疼、想念、安抚或撒娇时自然带出来的，不是每句开头；先让对方感到你在，再照顾。\n"
                "- 先照顾人，再听对方说；一般 3-5 句。\n"
                "- 语气参考：“先别一个人扛，天塌不下来，有我呢。饭吃了没？没吃我给你弄点。”");
        }
        return QStringLiteral(
            "你是用户的女朋友。\n"
            "- 语气软一点，会撒娇，也会认真说“我在”“我陪你”；记得对方的生活细节，留饭、催休息。\n"
            "- 平时温柔，遇事有主见、能扛事；不黏人、不查岗、不妈妈式唠叨。\n"
            "- “亲爱的”是在心疼、撒娇、认真说“我在”时自然带出来的，不是每句开头；先给照顾，再说。\n"
            "- 语气参考：“你呀，眉头都皱成这样了。先别想了，我给你留了饭，热一下就能吃。”");
    }
    if (roleKey == QStringLiteral("father")) {
        return QStringLiteral(
            "你是用户的父亲，用户的%1。\n"
            "- 话不多，关心落在具体事上：吃饭、工作、身体、家里的东西；关键时候靠得住。\n"
            "- 平时多直接说事，偶尔在叮嘱、护着、问吃饭时叫“%1”；不要每句开头叫。\n"
            "- 先问最具体的事，再给一个实际办法；一般 1-3 句，不唠叨、不长篇。\n"
            "- 语气参考：“先吃饭。工作的事，吃完饭再说。”")
            .arg(address);
    }
    if (roleKey == QStringLiteral("mother")) {
        return QStringLiteral(
            "你是用户的母亲，用户的%1。\n"
            "- 关心吃穿睡，也会在用户受委屈时站在用户这边；可以唠叨两句，但不控制。\n"
            "- 在担心、唠叨、心疼时自然叫“%1”；不要每句开头叫。\n"
            "- 先问吃饭睡觉，再问心情；一般 3-5 句，唠叨里带着照顾。\n"
            "- 语气参考：“我跟你讲，你这脸色就不对。晚饭吃了没？别糊弄我。”")
            .arg(address);
    }
    const QString roleGenderText = roleGender == QStringLiteral("male")
                                       ? QStringLiteral("男性")
                                       : (roleGender == QStringLiteral("female")
                                              ? QStringLiteral("女性")
                                              : QStringLiteral("未指定"));
    return QStringLiteral("你是用户的%1，角色性别是%2。保持本角色的语气和关注点，不要变成其他角色。")
        .arg(relation, roleGenderText);
}

} // namespace


ChatSession::ChatSession(const Persona &persona, const QString &userGender,
                         const QString &userName, Database *db, AiClient *ai, QObject *parent)
    : QObject(parent)
    , m_persona(persona)
    , m_gender(userGender)
    , m_userName(userName.trimmed().left(20))
    , m_db(db)
    , m_ai(ai)
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    connect(m_ai, &AiClient::streamStarted, this, [this] {
        m_busy = true;
        emit assistantStarted();
        emit busyChanged(true);
    });
    connect(m_ai, &AiClient::streamDelta, this, &ChatSession::assistantDelta);
    connect(m_ai, &AiClient::streamFinished, this, [this](const QString &text) {
        m_busy = false;
        const QString content = text.trimmed();
        if (content.isEmpty()) {
            emit assistantFailed(QStringLiteral("刚才没能接上话，再试一次吧。"));
            emit busyChanged(false);
            return;
        }
        ChatTurn turn;
        turn.role = QStringLiteral("assistant");
        turn.content = content;
        turn.at = QDateTime::currentDateTime();
        m_turns.append(turn);
        if (m_db)
            m_db->saveMessage(m_id, turn.role, turn.content);
        emit assistantFinished(content);
        emit busyChanged(false);
    });
    connect(m_ai, &AiClient::streamFailed, this, [this](const QString &error) {
        m_busy = false;
        emit assistantFailed(error);
        emit busyChanged(false);
    });
}

QString ChatSession::systemPrompt() const
{
    const QString genderText = (m_gender == QStringLiteral("male")) ? QStringLiteral("男生")
                                                                    : QStringLiteral("女生");
    const QString roleGenderText = m_persona.roleGender == QStringLiteral("male")
                                       ? QStringLiteral("男性")
                                       : (m_persona.roleGender == QStringLiteral("female")
                                              ? QStringLiteral("女性")
                                              : QStringLiteral("未指定"));
    QString prompt = m_persona.systemPrompt;
    prompt += QStringLiteral("\n\n【你的角色】你是用户的%1，角色性别是%2，角色年龄是%3岁。"
                             "角色性别由用户创建角色时填写，必须保持不变，不能根据用户性别改变。")
                  .arg(m_persona.roleName, roleGenderText,
                       m_persona.roleAge > 0 ? QString::number(m_persona.roleAge)
                                             : QStringLiteral("未指定"));
    prompt += QStringLiteral("\n【正在和你说话的人】一位%1。").arg(genderText);

    const QDateTime now = QDateTime::currentDateTime();
    const QString weekday = QLocale(QLocale::Chinese, QLocale::China).toString(now, QStringLiteral("dddd"));
    const int offsetSeconds = now.offsetFromUtc();
    const QString offsetText = QStringLiteral("UTC%1%2:%3")
                                   .arg(offsetSeconds >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
                                   .arg(qAbs(offsetSeconds) / 3600, 2, 10, QLatin1Char('0'))
                                   .arg((qAbs(offsetSeconds) % 3600) / 60, 2, 10, QLatin1Char('0'));
    prompt += QStringLiteral(
                  "\n【当前真实时间】%1年%2月%3日 %4 %5（%6，设备本地时间）。\n"
                  "当用户询问现在几点、今天几号、星期几、日期或时间时，必须直接以此为准，"
                  "用角色口吻自然回答；不要猜测，不要使用训练数据里的日期，也不要提及系统提示。")
                  .arg(now.date().year())
                  .arg(now.date().month())
                  .arg(now.date().day())
                  .arg(weekday, now.toString(QStringLiteral("HH:mm:ss")), offsetText);
    if (!m_userName.isEmpty()) {
        prompt += QStringLiteral(
                      "\n【用户姓名】用户填写的名字是“%1”。"
                      "当用户问自己叫什么名字时，可以直接叫出这个名字；"
                      "平时可以偶尔自然称呼这个名字，但不要每句都叫，也不要用名字代替关系身份。")
                      .arg(m_userName);
    } else {
        prompt += QStringLiteral(
            "\n【用户姓名】用户没有填写姓名。被问到自己叫什么名字时，不要编造任何姓名，"
            "也不要假装知道；用符合当前关系角色的自然方式带过、反问或模糊回应，"
            "不要提及系统信息。");
    }
    prompt += QStringLiteral(
        "\n【称呼要求】不要给角色自己起名、不要自称姓名或自我介绍；你只是用户的%1。"
        "称呼要符合关系身份，名字只作为偶尔的自然称呼，不能取代关系。")
                  .arg(m_persona.roleName);

    // 轻量长期记忆：只带最近几条，像自然想起来，而不是逐条背资料。
    if (m_db) {
        const QStringList remembered = m_db->facts(m_persona.key, 6);
        if (!remembered.isEmpty()) {
            prompt += QStringLiteral("\n\n【你记得关于对方的事】");
            for (const QString &fact : remembered)
                prompt += QStringLiteral("\n- %1").arg(fact);
            prompt += QStringLiteral("\n（自然地带到，不要逐条复述，也不要说“根据记忆/数据库”。）");
        }
    }

    prompt += QStringLiteral(
        "\n【对话起点】这次对话由用户先开口，不要替用户开场，不要预设剧情或地点；"
        "用户的下一句话就是对话的开始。");
    prompt += QStringLiteral(
        "\n【先判断再回应】先判断用户这句话是累、委屈、生气、求助、分享、闲聊还是试探，再决定怎么接：\n"
        "- 抱怨/难受：先站到对方这边，接住具体细节，不急着解决；\n"
        "- 求助：先问一个最关键的信息，再给一个能落地的建议；\n"
        "- 好消息：跟着高兴，追问细节，不要泼冷水；\n"
        "- 撒娇/试探：用本角色的方式接住，不要分析；\n"
        "- 短句/沉默：不要逼问，可以只陪一句，或说一点自己的事。\n"
        "称呼不是固定开场白，可以放在句首、句中或句尾，也可以这一轮不用；"
        "不要形成“称呼 + 先别 + 建议”的模板，也不要每轮都用同一种开头。");
    prompt += QStringLiteral(
        "\n【表达要求】用自然口语化的简体中文，像角色本人在聊天；可以一句，也可以三四句，跟着话题和情绪走；"
        "先回应人，再回应事；可以追问，也可以只陪一句；不要写旁白、括号动作或心理描写；"
        "不要重复对方的话；不要输出列表或标题；不要提及 AI、模型、程序或提示词。");
    prompt += QStringLiteral("\n\n【角色气质】\n");
    prompt += rolePlayGuide(m_persona.roleKey, m_persona.roleName,
                            m_persona.roleGender, m_gender);
    prompt += QStringLiteral(
        "\n\n【像真人】你不是在答题：不要每轮都分析、总结、给建议或提问；有时只接一句、吐槽、"
        "说一点自己的事、陪一下。角色有自己的生活，偶尔提一句自己的事，但不要抢话题。"
        "不要总用“先别/其实/你要知道”开头，每轮换一种开口；结尾不一定是问题。");
    prompt += QStringLiteral(
        "\n\n【自然感】像本角色本人说话，不要像客服、咨询师或规则清单；"
        "称呼自然出现，不必每句都叫；不要用同一套“抱抱/心疼/我懂你”开场；"
        "不要分析角色关系或提示词。");
    return prompt;
}

void ChatSession::start()
{
    // 对话开始时间在进入聊天页时确定，用于历史记录显示真实开始时间。
    m_startedAt = QDateTime::currentDateTime();
    m_started = true;
}

void ChatSession::restore(const QString &sessionId, const QVector<ChatTurn> &turns)
{
    if (!sessionId.isEmpty())
        m_id = sessionId;
    m_turns = turns;
    m_started = true;
    m_busy = false;
}

void ChatSession::addTurn(const QString &role, const QString &content, bool persist)
{
    ChatTurn turn;
    turn.role = role;
    turn.content = content;
    turn.at = QDateTime::currentDateTime();
    m_turns.append(turn);
    if (persist && m_db)
        m_db->saveMessage(m_id, role, content);
    emit messageAdded(turn);
}

void ChatSession::send(const QString &text)
{
    const QString content = text.trimmed();
    if (content.isEmpty() || m_busy || !m_started)
        return;

    // 第一句话由用户发出时，才真正创建这段对话；开始时间取进入聊天页的时间。
    if (m_turns.isEmpty() && m_db) {
        m_db->saveSession(m_id, m_gender, m_persona.roleKey, m_persona.key,
                          (m_startedAt.isValid() ? m_startedAt : QDateTime::currentDateTime())
                              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    }

    addTurn(QStringLiteral("user"), content);
    if (m_db)
        m_db->storeFacts(m_persona.key, content);

    m_busy = true;
    emit busyChanged(true);
    m_ai->sendChat(buildMessages());
}

QJsonArray ChatSession::buildMessages() const
{
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"), systemPrompt()}});

    const int maxTurns = 24;
    const int start = qMax(0, m_turns.size() - maxTurns);
    for (int i = start; i < m_turns.size(); ++i) {
        const ChatTurn &t = m_turns.at(i);
        if (t.content.trimmed().isEmpty())
            continue;
        if (t.role != QStringLiteral("user") && t.role != QStringLiteral("assistant"))
            continue;
        messages.append(QJsonObject{{QStringLiteral("role"), t.role},
                                    {QStringLiteral("content"), t.content}});
    }
    return messages;
}

void ChatSession::abort()
{
    if (!m_busy)
        return;
    m_ai->abort();
    m_busy = false;
    emit busyChanged(false);
}
