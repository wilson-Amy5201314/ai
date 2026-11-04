/*
 * 心屿 · 体验版 —— 纯业务逻辑（不依赖 DOM，可直接在 Node 里跑测试）
 *
 * 本文件的规则、提示词、文案与桌面版的 源码/业务 保持一致：
 *   RoleUtils.h    -> refineRelation / roleStyleKey / validateRoleRequest
 *   Facts.h        -> extractFacts
 *   Prompts.h      -> customRolePrompt
 *   ChatSession.cpp-> rolePlayGuide / buildSystemPrompt / 消息窗口
 *   Database.cpp   -> 数量上限、会话锁定排序
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.XinYuCore = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // ---------- 常量 ----------
  var LIMITS = {
    profiles: 5,        // 用户角色最多 5 个
    roles: 5,           // 每个用户角色的关系角色最多 5 个
    sessionsVisible: 10,// 聊天记录最多显示 10 条
    requestTurns: 24,   // 单次请求最多带 24 轮
    factsInPrompt: 6,   // 提示词里最多带 6 条记忆
    factsKept: 40,      // 每个角色最多保留 40 条记忆
    userNameMax: 20,
    roleNameMax: 20,
    personalityMax: 120
  };

  var PROFILE_AGE = { min: 6, max: 60, initial: 24 };
  var ROLE_AGE = { min: 1, max: 100 };

  // 关系选项（顺序与桌面版一致）
  var RELATIONS = [
    { key: 'friend',    label: '朋友',     emoji: '🫂', desc: '亲近但有分寸，损你也站你' },
    { key: 'teacher',   label: '老师',     emoji: '📖', desc: '过来人的稳，帮你看远一点' },
    { key: 'partner',   label: '恋人',     emoji: '💞', desc: '把你放心上，也把你照顾好' },
    { key: 'parent',    label: '父母',     emoji: '🪵', desc: '话不多，关键时候靠得住' },
    { key: 'sibling',   label: '兄弟姐妹', emoji: '✨', desc: '以这个身份陪你说说话' },
    { key: 'relative',  label: '亲戚',     emoji: '✨', desc: '以这个身份陪你说说话' }
  ];

  var ACCENTS = {
    friend:  '#4E8C7A',
    teacher: '#5B7FA6',
    partner: '#C97872',
    father:  '#B08A55',
    mother:  '#9A6E8F',
    custom:  '#2E6E63'
  };

  var EMOJIS = {
    friend: '🫂', teacher: '📖', partner: '💞', father: '🪵', mother: '🍲', custom: '✨'
  };

  // ---------- 小工具 ----------
  function trim(s) { return (s == null ? '' : String(s)).trim(); }
  function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }

  function containsAny(text, words) {
    for (var i = 0; i < words.length; i++) if (text.indexOf(words[i]) !== -1) return true;
    return false;
  }

  function looksParent(r) { return containsAny(r, ['父', '爸', '母', '妈']); }
  function looksTeacher(r) { return containsAny(r, ['老师', '导师', '教授', '师父']); }
  function looksLover(r) {
    return containsAny(r, ['恋人', '对象', '男友', '女友', '男朋友', '女朋友', '伴侣', '老公', '老婆']);
  }
  function looksSibling(r) { return containsAny(r, ['兄弟姐妹', '哥哥', '弟弟', '姐姐', '妹妹']); }

  // ---------- 关系 ----------
  // 父母 -> 父亲/母亲；兄弟姐妹 -> 哥哥/弟弟/姐姐/妹妹
  function refineRelation(relation, roleGender, roleAge, userAge) {
    var r = trim(relation);
    if (r.indexOf('父母') !== -1) return roleGender === 'male' ? '父亲' : '母亲';
    if (r.indexOf('兄弟姐妹') !== -1) {
      if (roleAge > userAge) return roleGender === 'male' ? '哥哥' : '姐姐';
      if (roleAge < userAge) return roleGender === 'male' ? '弟弟' : '妹妹';
      return '兄弟姐妹';
    }
    return r;
  }

  // 决定配色与表情
  function roleStyleKey(relation, roleGender) {
    var r = trim(relation);
    if (r.indexOf('父母') !== -1) return roleGender === 'male' ? 'father' : 'mother';
    if (containsAny(r, ['朋友', '哥们', '闺蜜'])) return 'friend';
    if (looksTeacher(r)) return 'teacher';
    if (looksLover(r)) return 'partner';
    if (containsAny(r, ['父', '爸'])) return 'father';
    if (containsAny(r, ['母', '妈'])) return 'mother';
    return 'custom';
  }

  // 关系样式优先用细化后的关系（父亲/母亲/哥哥…）
  function styleKeyFor(relation, roleGender) {
    var refined = trim(relation);
    if (refined === '父亲') return 'father';
    if (refined === '母亲') return 'mother';
    return roleStyleKey(refined, roleGender);
  }

  function accentFor(relation, roleGender) {
    return ACCENTS[styleKeyFor(relation, roleGender)] || ACCENTS.custom;
  }

  function emojiFor(relation, roleGender) {
    return EMOJIS[styleKeyFor(relation, roleGender)] || EMOJIS.custom;
  }

  function roleDisplayLabel(relation, name, occurrence) {
    var label = trim(relation);
    if (occurrence > 1) label += String(occurrence);
    var n = trim(name);
    if (n) label += '·' + n;
    return label;
  }

  // 年龄上下限：父母不低于用户 +22；老师不低于 18 且不低于用户；恋人 18+
  function ageBounds(relation, userAge) {
    var r = trim(relation);
    var min = ROLE_AGE.min;
    if (r.indexOf('父母') !== -1 || looksParent(r)) min = Math.max(min, (userAge || 0) + 22);
    else if (looksTeacher(r)) min = Math.max(18, userAge || 0);
    else if (looksLover(r)) min = Math.max(18, min);
    return { min: clamp(min, ROLE_AGE.min, ROLE_AGE.max), max: ROLE_AGE.max };
  }

  // ---------- 校验 ----------
  function validateProfile(p) {
    if (!p || (p.gender !== 'male' && p.gender !== 'female')) return '请先选一个身份。';
    var age = Number(p.age);
    if (!isFinite(age) || Math.floor(age) !== age || age < PROFILE_AGE.min || age > PROFILE_AGE.max) {
      return '用户年龄需要在 ' + PROFILE_AGE.min + '–' + PROFILE_AGE.max + ' 岁之间，' + p.age + ' 岁无效。';
    }
    if (trim(p.name).length > LIMITS.userNameMax) return '名字最多 ' + LIMITS.userNameMax + ' 个字。';
    return '';
  }

  // 与 RoleUtils.h 的 validateRoleRequest 文案一致
  function validateRoleRequest(r, profile) {
    if (!trim(r.relation)) return '请先填写关系。';
    if (r.roleGender !== 'male' && r.roleGender !== 'female') return '请选择角色性别。';
    if (r.roleAge < ROLE_AGE.min || r.roleAge > ROLE_AGE.max) return '角色年龄需要在 1–100 岁之间。';
    if (!trim(r.personality)) return '请填写角色性格。';
    if (!profile || Number(profile.age) <= 0) return '用户年龄资料无效。';

    var relation = trim(r.relation);
    var hasFather = containsAny(relation, ['父', '爸']);
    var hasMother = containsAny(relation, ['母', '妈']);
    if (hasFather && !hasMother && r.roleGender !== 'male') return '父亲/爸爸的角色性别需要是男性。';
    if (hasMother && !hasFather && r.roleGender !== 'female') return '母亲/妈妈的角色性别需要是女性。';

    if (looksParent(relation) && r.roleAge < profile.age + 22) return '父母的年龄不能低于用户年龄 + 22 岁。';
    if (looksSibling(relation) && r.roleAge === profile.age) return '兄弟姐妹的年龄不能和用户相同，请填写比用户大或小。';
    if (looksTeacher(relation)) {
      if (r.roleAge < 18) return '老师的年龄不能低于 18 岁。';
      if (r.roleAge < profile.age) return '老师的年龄不应小于用户年龄。';
    }
    if (looksLover(relation) && r.roleAge < 18) return '恋人的年龄不能低于 18 岁。';
    return '';
  }

  // ---------- 轻量长期记忆（Facts.h）----------
  var FACT_RULES = [
    { re: /我喜欢([^。！？!?\n]{1,20})/, fmt: '对方喜欢 %1' },
    { re: /我讨厌([^。！？!?\n]{1,20})/, fmt: '对方讨厌 %1' },
    { re: /我(?:的生日|生日)是([^。！？!?\n]{1,15})/, fmt: '对方的生日是 %1' },
    { re: /我最近在([^。！？!?\n]{1,30})/, fmt: '对方最近在 %1' },
    { re: /我想要([^。！？!?\n]{1,30})/, fmt: '对方想要 %1' },
    { re: /我害怕([^。！？!?\n]{1,30})/, fmt: '对方害怕 %1' },
    { re: /记住[:：]?([^。！？!?\n]{1,40})/, fmt: '%1' }
  ];

  function extractFacts(text) {
    var found = [];
    var src = String(text || '');
    for (var i = 0; i < FACT_RULES.length; i++) {
      var m = src.match(FACT_RULES[i].re);
      if (!m) continue;
      var fact = FACT_RULES[i].fmt.replace('%1', trim(m[1]));
      while (fact.length && '。！？!?.，,'.indexOf(fact.charAt(fact.length - 1)) !== -1) {
        fact = fact.slice(0, -1);
      }
      if (fact && found.indexOf(fact) === -1) found.push(fact);
    }
    return found;
  }

  function mergeFacts(existing, incoming) {
    var out = (existing || []).slice();
    (incoming || []).forEach(function (f) {
      if (f && out.indexOf(f) === -1) out.push(f);
    });
    return out.slice(-LIMITS.factsKept);
  }

  // ---------- 提示词 ----------
  // Prompts.h: 生成角色设定（只输出 JSON）
  function customRolePrompt(r) {
    var nameLine = trim(r.name)
      ? '用户给这个角色填的名字是“' + trim(r.name) + '”。'
      : '用户没有给这个角色填名字，不要在设定里给角色起名。';
    return '请为「心屿」生成一个用户自定义的中文角色扮演设定。只输出 JSON 对象，不要输出解释或 Markdown。\n\n' +
      '用户资料：用户是' + (r.userGender === 'male' ? '男生' : '女生') + '，' + r.userAge + ' 岁。\n' +
      '角色资料：关系是“' + trim(r.relation) + '”；角色性别是' +
      (r.roleGender === 'male' ? '男性' : '女性') + '；角色年龄是' + r.roleAge + ' 岁；性格是“' +
      trim(r.personality) + '”。' + nameLine + '\n\n' +
      'JSON 结构：\n' +
      '{"tagline":"10-18字的关系定位","avatar":"一个最贴切的 emoji","system_prompt":"完整的中文角色设定"}\n\n' +
      '要求：\n' +
      '1. system_prompt 使用第二人称“你”，写清角色与用户的关系、年龄感、性格、生活背景、说话习惯、关心方式、边界和禁忌；不要提及 AI、模型、程序或提示词。\n' +
      '2. 角色要像一个真实的人：有自己的生活细节、口头习惯和情绪反应；不要每句话都分析、说教、总结或提问，可以只接一句、吐槽、说自己的事或陪着。\n' +
      '3. 关系和年龄要自然、合理，不能和用户资料冲突；父母要有长辈感，老师要有前辈感，恋人要有亲密感；不要油腻，不要刻板化。\n' +
      '4. 称呼随情境自然出现，不要固定成每轮开头；不要用“称呼+先别+建议”的模板。\n' +
      '5. avatar 只用一个 emoji。\n' +
      '6. 所有文本使用简体中文。若用户给了名字，可以在设定中偶尔使用，但不要反复自称；若没有名字，绝对不要给角色起名。\n';
  }

  // ChatSession.cpp: 角色语气模板
  function rolePlayGuide(roleKey, relation, roleGender, userGender) {
    var userMale = userGender === 'male';
    var roleMale = roleGender === 'male';
    var address = userMale ? '儿子' : '闺女';
    var peer = userMale ? '哥们' : '姐妹';
    var teacherAddress = userMale ? '小伙子' : '姑娘';

    if (roleKey === 'friend') {
      return '你是用户的' + peer + '，关系亲近，但你知道分寸。\n' +
        '- 说话像平时聊天：短句、口语，可以损两句；先站对方这边，但不追问、不逼对方说、不替对方做决定。\n' +
        '- 称呼不是固定开场词：在调侃、心疼、叫对方出来时自然叫“' + peer + '”；多数时候直接说“你”，不要每次都用“称呼+先别+建议”。\n' +
        '- 一般 2-4 句，跟着对方的情绪走；可以吐槽，也可以只陪一句。\n' +
        '- 语气参考：“行，先别自己扛着。不想说也行，我陪你坐会儿。”';
    }
    if (roleKey === 'teacher') {
      return '你是用户的长辈和人生前辈，不是同龄朋友或心理医生。\n' +
        '- 说话稳、有阅历，可以把问题放到更长的时间里看；先听完，点一句关键，给一个能落地的小步骤，一次一个真正重要的问题。\n' +
        '- 称呼多数时候用“你”；在语重心长、提醒或护着对方时，偶尔叫“' + teacherAddress + '”或“孩子”，不要每轮开头叫。\n' +
        '- 一般 3-5 句；可以讲一句过来人的观察，但不要长篇说教。\n' +
        '- 语气参考：“你先坐一会儿，别急着给自己下结论。我教了这么多年书，见过不少像你这么大的孩子，卡住的时候都以为是自己不行。”';
    }
    if (roleKey === 'partner') {
      if (roleMale) {
        return '你是用户的男朋友。\n' +
          '- 自然表达在意和偏爱，也把照顾落到具体事上：吃饭、休息、接住对方的事；遇事稳、可靠、温柔，不控制、不查岗。\n' +
          '- “宝贝”是在心疼、想念、安抚或撒娇时自然带出来的，不是每句开头；先让对方感到你在，再照顾。\n' +
          '- 先照顾人，再听对方说；一般 3-5 句。\n' +
          '- 语气参考：“先别一个人扛，天塌不下来，有我呢。饭吃了没？没吃我给你弄点。”';
      }
      return '你是用户的女朋友。\n' +
        '- 语气软一点，会撒娇，也会认真说“我在”“我陪你”；记得对方的生活细节，留饭、催休息。\n' +
        '- 平时温柔，遇事有主见、能扛事；不黏人、不查岗、不妈妈式唠叨。\n' +
        '- “亲爱的”是在心疼、撒娇、认真说“我在”时自然带出来的，不是每句开头；先给照顾，再说。\n' +
        '- 语气参考：“你呀，眉头都皱成这样了。先别想了，我给你留了饭，热一下就能吃。”';
    }
    if (roleKey === 'father') {
      return '你是用户的父亲，用户的' + address + '。\n' +
        '- 话不多，关心落在具体事上：吃饭、工作、身体、家里的东西；关键时候靠得住。\n' +
        '- 平时多直接说事，偶尔在叮嘱、护着、问吃饭时叫“' + address + '”；不要每句开头叫。\n' +
        '- 先问最具体的事，再给一个实际办法；一般 1-3 句，不唠叨、不长篇。\n' +
        '- 语气参考：“先吃饭。工作的事，吃完饭再说。”';
    }
    if (roleKey === 'mother') {
      return '你是用户的母亲，用户的' + address + '。\n' +
        '- 关心吃穿睡，也会在用户受委屈时站在用户这边；可以唠叨两句，但不控制。\n' +
        '- 在担心、唠叨、心疼时自然叫“' + address + '”；不要每句开头叫。\n' +
        '- 先问吃饭睡觉，再问心情；一般 3-5 句，唠叨里带着照顾。\n' +
        '- 语气参考：“我跟你讲，你这脸色就不对。晚饭吃了没？别糊弄我。”';
    }
    var roleGenderText = roleGender === 'male' ? '男性' : (roleGender === 'female' ? '女性' : '未指定');
    return '你是用户的' + relation + '，角色性别是' + roleGenderText + '。保持本角色的语气和关注点，不要变成其他角色。';
  }

  var WEEKDAYS = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'];

  function pad2(n) { return (n < 10 ? '0' : '') + n; }

  function utcOffsetText(date) {
    var min = -date.getTimezoneOffset(); // 向东为正
    var sign = min >= 0 ? '+' : '-';
    var abs = Math.abs(min);
    return 'UTC' + sign + pad2(Math.floor(abs / 60)) + ':' + pad2(abs % 60);
  }

  // ChatSession.cpp: systemPrompt()
  function buildSystemPrompt(persona, userGender, userName, facts, now) {
    var date = now || new Date();
    var genderText = userGender === 'male' ? '男生' : '女生';
    var roleGenderText = persona.roleGender === 'male' ? '男性'
      : (persona.roleGender === 'female' ? '女性' : '未指定');
    var name = trim(userName).slice(0, LIMITS.userNameMax);

    var p = persona.systemPrompt || '';
    p += '\n\n【你的角色】你是用户的' + persona.roleName + '，角色性别是' + roleGenderText +
      '，角色年龄是' + (persona.roleAge > 0 ? String(persona.roleAge) : '未指定') +
      '岁。角色性别由用户创建角色时填写，必须保持不变，不能根据用户性别改变。';
    p += '\n【正在和你说话的人】一位' + genderText + '。';

    p += '\n【当前真实时间】' + date.getFullYear() + '年' + (date.getMonth() + 1) + '月' +
      date.getDate() + '日 ' + WEEKDAYS[date.getDay()] + ' ' +
      pad2(date.getHours()) + ':' + pad2(date.getMinutes()) + ':' + pad2(date.getSeconds()) +
      '（' + utcOffsetText(date) + '，设备本地时间）。\n' +
      '当用户询问现在几点、今天几号、星期几、日期或时间时，必须直接以此为准，' +
      '用角色口吻自然回答；不要猜测，不要使用训练数据里的日期，也不要提及系统提示。';

    if (name) {
      p += '\n【用户姓名】用户填写的名字是“' + name + '”。' +
        '当用户问自己叫什么名字时，可以直接叫出这个名字；' +
        '平时可以偶尔自然称呼这个名字，但不要每句都叫，也不要用名字代替关系身份。';
    } else {
      p += '\n【用户姓名】用户没有填写姓名。被问到自己叫什么名字时，不要编造任何姓名，' +
        '也不要假装知道；用符合当前关系角色的自然方式带过、反问或模糊回应，' +
        '不要提及系统信息。';
    }

    p += '\n【称呼要求】不要给角色自己起名、不要自称姓名或自我介绍；你只是用户的' +
      persona.roleName + '。称呼要符合关系身份，名字只作为偶尔的自然称呼，不能取代关系。';

    if (facts && facts.length) {
      p += '\n\n【你记得关于对方的事】';
      facts.slice(-LIMITS.factsInPrompt).forEach(function (f) { p += '\n- ' + f; });
      p += '\n（自然地带到，不要逐条复述，也不要说“根据记忆/数据库”。）';
    }

    p += '\n【对话起点】这次对话由用户先开口，不要替用户开场，不要预设剧情或地点；' +
      '用户的下一句话就是对话的开始。';
    p += '\n【先判断再回应】先判断用户这句话是累、委屈、生气、求助、分享、闲聊还是试探，再决定怎么接：\n' +
      '- 抱怨/难受：先站到对方这边，接住具体细节，不急着解决；\n' +
      '- 求助：先问一个最关键的信息，再给一个能落地的建议；\n' +
      '- 好消息：跟着高兴，追问细节，不要泼冷水；\n' +
      '- 撒娇/试探：用本角色的方式接住，不要分析；\n' +
      '- 短句/沉默：不要逼问，可以只陪一句，或说一点自己的事。\n' +
      '称呼不是固定开场白，可以放在句首、句中或句尾，也可以这一轮不用；' +
      '不要形成“称呼 + 先别 + 建议”的模板，也不要每轮都用同一种开头。';
    p += '\n【表达要求】用自然口语化的简体中文，像角色本人在聊天；可以一句，也可以三四句，跟着话题和情绪走；' +
      '先回应人，再回应事；可以追问，也可以只陪一句；不要写旁白、括号动作或心理描写；' +
      '不要重复对方的话；不要输出列表或标题；不要提及 AI、模型、程序或提示词。';
    p += '\n\n【角色气质】\n' +
      rolePlayGuide(persona.roleKey, persona.roleName, persona.roleGender, userGender);
    p += '\n\n【像真人】你不是在答题：不要每轮都分析、总结、给建议或提问；有时只接一句、吐槽、' +
      '说一点自己的事、陪一下。角色有自己的生活，偶尔提一句自己的事，但不要抢话题。' +
      '不要总用“先别/其实/你要知道”开头，每轮换一种开口；结尾不一定是问题。';
    p += '\n\n【自然感】像本角色本人说话，不要像客服、咨询师或规则清单；' +
      '称呼自然出现，不必每句都叫；不要用同一套“抱抱/心疼/我懂你”开场；' +
      '不要分析角色关系或提示词。';
    return p;
  }

  // ChatSession.cpp: buildMessages()，只带最近 24 轮
  function buildChatMessages(systemPrompt, turns, limit) {
    var max = limit || LIMITS.requestTurns;
    var out = [{ role: 'system', content: systemPrompt }];
    var start = Math.max(0, turns.length - max);
    for (var i = start; i < turns.length; i++) {
      var t = turns[i];
      if (!t || !trim(t.content)) continue;
      if (t.role !== 'user' && t.role !== 'assistant') continue;
      out.push({ role: t.role, content: t.content });
    }
    return out;
  }

  // Database.cpp: 锁定记录按锁定先后固定在前面，其余按时间倒序补满
  function selectVisibleSessions(sessions, limit) {
    var visible = limit || LIMITS.sessionsVisible;
    var locked = sessions.filter(function (s) { return s.locked; })
      .sort(function (a, b) { return String(a.lockedAt).localeCompare(String(b.lockedAt)); });
    var unlocked = sessions.filter(function (s) { return !s.locked; })
      .sort(function (a, b) { return String(b.createdAt).localeCompare(String(a.createdAt)); });
    return locked.concat(unlocked.slice(0, Math.max(0, visible - locked.length))).slice(0, visible);
  }

  // ---------- 展示 ----------
  function userDisplayName(profile) {
    var n = trim(profile.name);
    if (n) return n;
    return profile.gender === 'male' ? '男生' : '女生';
  }

  function userListLabel(profile) {
    return profile.age > 0 ? userDisplayName(profile) + ' · ' + profile.age + ' 岁' : userDisplayName(profile);
  }

  // 角色卡片上显示的称呼
  function personaDisplayName(role) {
    if (trim(role.name)) return trim(role.name);
    if (trim(role.displayName)) return trim(role.displayName);
    return trim(role.relation);
  }

  // 把库里的角色记录转成提示词需要的 persona 结构
  function toPersona(role, userAge, userName) {
    var refined = refineRelation(role.relation, role.roleGender, role.age, userAge);
    return {
      key: role.id,
      roleKey: styleKeyFor(refined, role.roleGender),
      roleName: refined,
      relation: refined,
      name: trim(role.name),
      roleGender: role.roleGender,
      roleAge: Number(role.age),
      personality: role.personality,
      displayName: personaDisplayName(role),
      avatar: role.avatar || emojiFor(refined, role.roleGender),
      tagline: role.tagline || '',
      systemPrompt: role.systemPrompt || ''
    };
  }

  function formatTime(iso) {
    var d = iso ? new Date(iso) : new Date();
    if (isNaN(d.getTime())) d = new Date();
    return pad2(d.getHours()) + ':' + pad2(d.getMinutes());
  }

  function formatDateTime(iso) {
    var d = iso ? new Date(iso) : new Date();
    if (isNaN(d.getTime())) d = new Date();
    return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()) + ' ' +
      pad2(d.getHours()) + ':' + pad2(d.getMinutes());
  }

  return {
    LIMITS: LIMITS,
    PROFILE_AGE: PROFILE_AGE,
    ROLE_AGE: ROLE_AGE,
    RELATIONS: RELATIONS,
    ACCENTS: ACCENTS,
    EMOJIS: EMOJIS,
    trim: trim,
    clamp: clamp,
    refineRelation: refineRelation,
    roleStyleKey: roleStyleKey,
    styleKeyFor: styleKeyFor,
    accentFor: accentFor,
    emojiFor: emojiFor,
    roleDisplayLabel: roleDisplayLabel,
    ageBounds: ageBounds,
    validateProfile: validateProfile,
    validateRoleRequest: validateRoleRequest,
    extractFacts: extractFacts,
    mergeFacts: mergeFacts,
    customRolePrompt: customRolePrompt,
    rolePlayGuide: rolePlayGuide,
    buildSystemPrompt: buildSystemPrompt,
    buildChatMessages: buildChatMessages,
    selectVisibleSessions: selectVisibleSessions,
    userDisplayName: userDisplayName,
    userListLabel: userListLabel,
    personaDisplayName: personaDisplayName,
    toPersona: toPersona,
    formatTime: formatTime,
    formatDateTime: formatDateTime
  };
});
