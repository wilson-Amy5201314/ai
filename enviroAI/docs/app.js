/* 心屿 · 体验版 —— 界面与 AI 调用
 *
 * 数据存在浏览器 localStorage，结构对应桌面版的 SQLite 表：
 *   user_profiles / roles / sessions / messages / facts
 *
 * AI：浏览器直接请求 DeepSeek 接口。
 *   - 成功：真实生成与流式回复。
 *   - 失败（跨域被拦、断网、额度、密钥无效等）：自动降级为本地演示回复，
 *     页面右上角出现「演示模式」，其余功能照常可用。
 */
(function () {
  'use strict';

  // ==================== 连接配置 ====================
  // 注意：这个文件会随 GitHub Pages 公开，写在这里的密钥任何人都能看到。
  // 如果仓库是公开的，请换成自己的密钥，并在提交前确认额度风险。
  // 若浏览器被跨域拦下（控制台出现 CORS 报错），把 proxyUrl 改成自建代理地址，
  // 例如 Cloudflare Worker 转发到 https://api.deepseek.com/chat/completions。
  var CONFIG = {
    endpoint: 'https://api.deepseek.com/chat/completions',
    proxyUrl: '',
    apiKey: 'sk-810f77abf26a4a2fb74ac52c55a769dd',
    model: 'deepseek-chat'
  };

  var C = window.XinYuCore;
  var STORE_KEY = 'xinyu.web.v1';

  // ==================== 状态 ====================
  var state = {
    profiles: [],
    roles: [],
    sessions: [],
    messages: [],
    facts: [],
    profileId: null,
    roleId: null,
    sessionId: null,
    enteredAt: null,
    demoMode: false,
    busy: false,
    view: 'welcome'
  };

  function nowIso() { return new Date().toISOString(); }

  function uid() {
    if (window.crypto && crypto.randomUUID) return crypto.randomUUID();
    return 'id-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 9);
  }

  // ==================== 存储 ====================
  function save() {
    try {
      localStorage.setItem(STORE_KEY, JSON.stringify({
        profiles: state.profiles,
        roles: state.roles,
        sessions: state.sessions,
        messages: state.messages,
        facts: state.facts
      }));
    } catch (e) {
      toast('本地存储不可用，本次改动不会被记住');
    }
  }

  function load() {
    var raw = null;
    try { raw = localStorage.getItem(STORE_KEY); } catch (e) { raw = null; }
    if (!raw) return;
    try {
      var data = JSON.parse(raw);
      state.profiles = data.profiles || [];
      state.roles = data.roles || [];
      state.sessions = data.sessions || [];
      state.messages = data.messages || [];
      state.facts = data.facts || [];
    } catch (e) {
      toast('本地数据读取失败，已从空状态开始');
    }
  }

  // ==================== 小工具 ====================
  function $(id) { return document.getElementById(id); }
  function el(tag, cls, text) {
    var node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text != null) node.textContent = text;
    return node;
  }

  var toastTimer = null;
  function toast(msg) {
    var t = $('toast');
    t.textContent = msg;
    t.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { t.hidden = true; }, 3600);
  }

  function currentProfile() {
    for (var i = 0; i < state.profiles.length; i++) {
      if (state.profiles[i].id === state.profileId) return state.profiles[i];
    }
    return null;
  }

  function currentRole() {
    for (var i = 0; i < state.roles.length; i++) {
      if (state.roles[i].id === state.roleId) return state.roles[i];
    }
    return null;
  }

  function currentSession() {
    for (var i = 0; i < state.sessions.length; i++) {
      if (state.sessions[i].id === state.sessionId) return state.sessions[i];
    }
    return null;
  }

  function rolesOf(profileId) {
    return state.roles.filter(function (r) { return r.profileId === profileId; });
  }

  function sessionsOf(roleId) {
    return state.sessions.filter(function (s) { return s.personaKey === roleId; });
  }

  function messagesOf(sessionId) {
    return state.messages.filter(function (m) { return m.sessionId === sessionId; })
      .sort(function (a, b) { return a.id - b.id; });
  }

  function factsOf(roleId) {
    return state.facts.filter(function (f) { return f.personaKey === roleId; })
      .map(function (f) { return f.fact; });
  }

  function showView(name) {
    state.view = name;
    $('viewWelcome').hidden = name !== 'welcome';
    $('viewRoles').hidden = name !== 'roles';
    $('viewChat').hidden = name !== 'chat';
    window.scrollTo({ top: 0, behavior: 'auto' });
  }

  function setModeChip() {
    $('modeChip').hidden = !state.demoMode;
    $('memoryChip').textContent = state.demoMode ? '● 记忆已连接（本地）' : '● 记忆已连接';
  }

  // ==================== 开始页 ====================
  var pendingGender = '';

  function bindWelcome() {
    var cards = document.querySelectorAll('#viewWelcome .choiceCard[data-gender]');
    Array.prototype.forEach.call(cards, function (card) {
      card.addEventListener('click', function () {
        pendingGender = card.getAttribute('data-gender');
        Array.prototype.forEach.call(cards, function (c) { c.classList.toggle('selected', c === card); });
        $('enterBtn').disabled = false;
      });
    });

    bindStepper($('userAgeStepper'), $('userAge'), function () {
      return { min: C.PROFILE_AGE.min, max: C.PROFILE_AGE.max };
    }, function (value) {
      if (value < C.PROFILE_AGE.min || value > C.PROFILE_AGE.max) {
        toast('用户年龄需要在 ' + C.PROFILE_AGE.min + '–' + C.PROFILE_AGE.max + ' 岁之间，' + value + ' 岁无效。');
      }
    });

    $('enterBtn').addEventListener('click', function () {
      var req = {
        gender: pendingGender,
        age: Number($('userAge').value),
        name: C.trim($('userName').value)
      };
      var err = C.validateProfile(req);
      if (err) { toast(err); return; }
      if (state.profiles.length >= C.LIMITS.profiles) {
        toast('用户角色已满 ' + C.LIMITS.profiles + ' 个，请先删除一个');
        return;
      }
      var profile = {
        id: uid(),
        gender: req.gender,
        name: req.name,
        age: req.age,
        createdAt: nowIso()
      };
      state.profiles.unshift(profile);
      save();
      enterProfile(profile.id);
    });
  }

  // 数字步进器：点击加减 + 直接输入越界时提示并修正
  function bindStepper(container, input, getRange, onOutOfRange) {
    var buttons = container.querySelectorAll('.stepBtn');

    function apply(delta) {
      var range = getRange();
      var value = Number(input.value) || range.min;
      var next = C.clamp(value + delta, range.min, range.max);
      input.value = String(next);
      refresh();
    }

    function refresh() {
      var range = getRange();
      var value = Number(input.value) || range.min;
      Array.prototype.forEach.call(buttons, function (btn) {
        var delta = Number(btn.getAttribute('data-step'));
        btn.disabled = (delta < 0 && value <= range.min) || (delta > 0 && value >= range.max);
      });
    }

    Array.prototype.forEach.call(buttons, function (btn) {
      btn.addEventListener('click', function () { apply(Number(btn.getAttribute('data-step'))); });
    });

    input.addEventListener('change', function () {
      var range = getRange();
      var raw = input.value;
      var value = parseInt(raw, 10);
      if (!isFinite(value)) {
        input.value = String(range.min);
        refresh();
        return;
      }
      if (value < range.min || value > range.max) {
        if (onOutOfRange) onOutOfRange(value);
        value = C.clamp(value, range.min, range.max);
      }
      input.value = String(value);
      refresh();
    });

    input.addEventListener('input', function () {
      input.value = input.value.replace(/[^\d]/g, '').slice(0, 3);
    });

    refresh();
    return { refresh: refresh };
  }

  function renderProfiles() {
    var host = $('profileList');
    host.innerHTML = '';
    if (!state.profiles.length) {
      var empty = el('p', 'muted', '还没有身份。先在上面选一个，进入心屿。');
      host.appendChild(empty);
    }
    state.profiles.forEach(function (p) {
      var card = el('button', 'idCard');
      card.type = 'button';
      card.setAttribute('data-id', p.id);

      var avatar = el('span', 'idAvatar ' + p.gender, p.gender === 'male' ? '👦' : '👧');
      var text = el('span', 'idText');
      text.appendChild(el('span', 'idName', C.userDisplayName(p)));
      text.appendChild(el('span', 'idSub', p.age + ' 岁 · ' + (p.gender === 'male' ? '男生' : '女生')));

      var del = el('button', 'cardDel', '✕');
      del.type = 'button';
      del.title = '删除这个身份';
      del.addEventListener('click', function (ev) {
        ev.stopPropagation();
        deleteProfile(p.id);
      });

      card.appendChild(avatar);
      card.appendChild(text);
      card.appendChild(del);
      card.addEventListener('click', function () { enterProfile(p.id); });
      host.appendChild(card);
    });

    $('profileLimit').hidden = state.profiles.length < C.LIMITS.profiles;
    $('enterBtn').disabled = !pendingGender || state.profiles.length >= C.LIMITS.profiles;
  }

  function deleteProfile(id) {
    if (!window.confirm('删除这个身份？它下面的关系角色和聊天记录会一起删除。')) return;
    var roleIds = rolesOf(id).map(function (r) { return r.id; });
    state.profiles = state.profiles.filter(function (p) { return p.id !== id; });
    state.roles = state.roles.filter(function (r) { return r.profileId !== id; });
    state.sessions = state.sessions.filter(function (s) { return roleIds.indexOf(s.personaKey) === -1; });
    state.messages = state.messages.filter(function (m) {
      return state.sessions.some(function (s) { return s.id === m.sessionId; });
    });
    state.facts = state.facts.filter(function (f) { return roleIds.indexOf(f.personaKey) === -1; });
    if (state.profileId === id) {
      state.profileId = null;
      state.roleId = null;
      state.sessionId = null;
    }
    save();
    renderProfiles();
    toast('已删除该身份');
  }

  function enterProfile(id) {
    state.profileId = id;
    state.roleId = null;
    state.sessionId = null;
    renderRoles();
    showView('roles');
  }

  // ==================== 角色页 ====================
  var selectedRoleId = null;

  function renderRoles() {
    var profile = currentProfile();
    if (!profile) { showView('welcome'); return; }

    $('identityChip').textContent = '你以「' + C.userDisplayName(profile) + ' · ' + profile.age + ' 岁」的身份进入';

    var relHost = $('relationList');
    relHost.innerHTML = '';
    C.RELATIONS.forEach(function (rel) {
      var card = el('button', 'relationCard');
      card.type = 'button';
      card.appendChild(el('span', 'relEmoji', rel.emoji));
      card.appendChild(el('span', 'relName', rel.label));
      card.appendChild(el('span', 'relDesc', rel.desc));
      card.addEventListener('click', function () { openRoleDialog(rel.label); });
      relHost.appendChild(card);
    });

    var roles = rolesOf(profile.id);
    var host = $('roleList');
    host.innerHTML = '';
    if (!roles.length) {
      host.appendChild(el('p', 'muted', '还没有角色。先在上面选一种关系，创建一个，再开始对话。'));
    }

    roles.forEach(function (role) {
      var refined = C.refineRelation(role.relation, role.roleGender, role.age, profile.age);
      var accent = C.accentFor(refined, role.roleGender);
      var card = el('div', 'roleCard');
      card.style.setProperty('--role-accent', accent);
      card.setAttribute('data-id', role.id);
      if (role.id === selectedRoleId) card.classList.add('selected');

      var head = el('div', 'bubbleRow');
      head.style.display = 'flex';
      head.style.alignItems = 'center';
      var avatar = el('span', 'roleAvatar', role.avatar || C.emojiFor(refined, role.roleGender));
      var title = el('span', 'idText');
      title.appendChild(el('span', 'roleName', role.name || refined));
      title.appendChild(el('span', 'roleMeta', refined + ' · ' + role.age + ' 岁 · ' +
        (role.roleGender === 'male' ? '男性' : '女性')));
      head.appendChild(avatar);
      head.appendChild(title);
      card.appendChild(head);

      card.appendChild(el('p', 'roleTagline', role.tagline || '以这个身份陪你说说话'));

      var detail = el('div', 'roleDetail');
      [['关系', refined], ['年龄', role.age + ' 岁'], ['性别', role.roleGender === 'male' ? '男性' : '女性'],
       ['性格', role.personality], ['名字', role.name || '未设置']].forEach(function (pair) {
        var row = el('div', 'roleDetailRow');
        row.appendChild(el('span', 'roleDetailKey', pair[0]));
        row.appendChild(el('span', 'roleDetailVal', pair[1]));
        detail.appendChild(row);
      });
      card.appendChild(detail);

      // 没有名字的角色：可以设置一次
      if (!C.trim(role.name)) {
        var once = el('div', 'nameOnce');
        var input = el('input');
        input.placeholder = '设置名字（只能设置一次）';
        input.maxLength = C.LIMITS.roleNameMax;
        var btn = el('button', null, '确定');
        btn.type = 'button';
        btn.addEventListener('click', function (ev) {
          ev.stopPropagation();
          var val = C.trim(input.value);
          if (!val) { toast('请先输入名字'); return; }
          role.name = val.slice(0, C.LIMITS.roleNameMax);
          save();
          renderRoles();
          toast('名字已设置，之后不能再改');
        });
        once.appendChild(input);
        once.appendChild(btn);
        card.appendChild(once);
      }

      var del = el('button', 'cardDel', '✕');
      del.type = 'button';
      del.title = '删除这个角色';
      del.addEventListener('click', function (ev) {
        ev.stopPropagation();
        deleteRole(role.id);
      });
      card.appendChild(del);

      card.addEventListener('click', function () {
        if (selectedRoleId === role.id) {
          selectedRoleId = null;
        } else {
          selectedRoleId = role.id;
        }
        renderRoles();
      });
      host.appendChild(card);
    });

    $('roleLimitHint').textContent = roles.length + ' / ' + C.LIMITS.roles +
      (roles.length >= C.LIMITS.roles ? '（已达上限）' : '');
    $('continueBtn').disabled = !selectedRoleId;
    $('deleteRoleBtn').disabled = !selectedRoleId;
  }

  function deleteRole(id) {
    if (!window.confirm('删除这个角色？它的聊天记录和记忆会一起删除。')) return;
    state.roles = state.roles.filter(function (r) { return r.id !== id; });
    var sessionIds = state.sessions.filter(function (s) { return s.personaKey === id; })
      .map(function (s) { return s.id; });
    state.sessions = state.sessions.filter(function (s) { return s.personaKey !== id; });
    state.messages = state.messages.filter(function (m) { return sessionIds.indexOf(m.sessionId) === -1; });
    state.facts = state.facts.filter(function (f) { return f.personaKey !== id; });
    if (selectedRoleId === id) selectedRoleId = null;
    save();
    renderRoles();
    toast('已删除该角色');
  }

  // ==================== 角色编辑弹窗 ====================
  var dialog = { relation: '', gender: '', submitting: false };

  function openRoleDialog(relation) {
    if (rolesOf(state.profileId).length >= C.LIMITS.roles) {
      toast('每个身份最多 ' + C.LIMITS.roles + ' 个角色，请先删除一个');
      return;
    }
    dialog.relation = relation || '朋友';
    dialog.gender = '';
    dialog.submitting = false;

    $('dlgError').hidden = true;
    $('dlgName').value = '';
    $('dlgPersonality').value = '';
    $('dlgSubmit').disabled = false;
    $('dlgSubmit').textContent = '生成角色';

    var chipHost = $('dlgRelations');
    chipHost.innerHTML = '';
    C.RELATIONS.forEach(function (rel) {
      var chip = el('button', null, rel.label);
      chip.type = 'button';
      chip.setAttribute('data-rel', rel.label);
      if (rel.label === dialog.relation) chip.classList.add('selected');
      chip.addEventListener('click', function () {
        selectRelation(rel.label);
      });
      chipHost.appendChild(chip);
    });

    var gCards = $('dlgRole').querySelectorAll('[data-rgender]');
    Array.prototype.forEach.call(gCards, function (c) { c.classList.remove('selected'); });

    selectRelation(dialog.relation);
    $('dlgRole').hidden = false;
    setTimeout(function () { $('dlgPersonality').focus(); }, 30);
  }

  function selectRelation(label) {
    dialog.relation = label;
    var chips = $('dlgRelations').querySelectorAll('button');
    Array.prototype.forEach.call(chips, function (c) {
      c.classList.toggle('selected', c.getAttribute('data-rel') === label);
    });

    var profile = currentProfile();
    var bounds = C.ageBounds(label, profile ? profile.age : 0);
    var hint = '范围 ' + bounds.min + '–' + bounds.max + ' 岁';
    if (bounds.min > C.ROLE_AGE.min) hint += '（按关系要求）';
    $('dlgAgeHint').textContent = hint;

    var input = $('roleAge');
    var v = Number(input.value) || bounds.min;
    input.value = String(C.clamp(v, bounds.min, bounds.max));
    updateRoleAgeButtons();
  }

  function updateRoleAgeButtons() {
    var profile = currentProfile();
    var bounds = C.ageBounds(dialog.relation, profile ? profile.age : 0);
    var value = Number($('roleAge').value) || bounds.min;
    var btns = $('roleAgeStepper').querySelectorAll('.stepBtn');
    Array.prototype.forEach.call(btns, function (btn) {
      var delta = Number(btn.getAttribute('data-step'));
      btn.disabled = (delta < 0 && value <= bounds.min) || (delta > 0 && value >= bounds.max);
    });
  }

  function bindRoleDialog() {
    $('dlgCancel').addEventListener('click', function () { $('dlgRole').hidden = true; });

    var gCards = $('dlgRole').querySelectorAll('[data-rgender]');
    Array.prototype.forEach.call(gCards, function (card) {
      card.addEventListener('click', function () {
        dialog.gender = card.getAttribute('data-rgender');
        Array.prototype.forEach.call(gCards, function (c) {
          c.classList.toggle('selected', c === card);
        });
      });
    });

    bindStepper($('roleAgeStepper'), $('roleAge'), function () {
      var profile = currentProfile();
      return C.ageBounds(dialog.relation, profile ? profile.age : 0);
    }, function (value) {
      toast('这个关系要求年龄在 ' + (currentProfile() ? C.ageBounds(dialog.relation, currentProfile().age).min : 1) + ' 岁以上，已修正');
    });

    $('dlgSubmit').addEventListener('click', submitRoleDialog);

    $('dlgRole').addEventListener('click', function (ev) {
      if (ev.target === $('dlgRole') && !dialog.submitting) $('dlgRole').hidden = true;
    });
  }

  function submitRoleDialog() {
    if (dialog.submitting) return;
    var profile = currentProfile();
    if (!profile) return;

    var req = {
      relation: dialog.relation,
      roleGender: dialog.gender,
      roleAge: Number($('roleAge').value),
      personality: C.trim($('dlgPersonality').value),
      name: C.trim($('dlgName').value),
      userGender: profile.gender,
      userAge: profile.age
    };

    var err = C.validateRoleRequest(req, profile);
    if (err) {
      $('dlgError').textContent = err;
      $('dlgError').hidden = false;
      return;
    }
    $('dlgError').hidden = true;

    dialog.submitting = true;
    $('dlgSubmit').disabled = true;
    $('dlgSubmit').textContent = '正在生成设定…';

    generateRole(req).then(function (generated) {
      var refined = C.refineRelation(req.relation, req.roleGender, req.roleAge, profile.age);
      var role = {
        id: uid(),
        profileId: profile.id,
        relation: req.relation,
        name: req.name,
        roleGender: req.roleGender,
        age: req.roleAge,
        personality: req.personality,
        systemPrompt: generated.systemPrompt,
        tagline: generated.tagline,
        avatar: generated.avatar,
        createdAt: nowIso()
      };
      state.roles.push(role);
      save();
      $('dlgRole').hidden = true;
      dialog.submitting = false;
      $('dlgSubmit').disabled = false;
      $('dlgSubmit').textContent = '生成角色';
      selectedRoleId = role.id;
      renderRoles();
      toast('角色已创建：' + refined);
    }).catch(function (e) {
      dialog.submitting = false;
      $('dlgSubmit').disabled = false;
      $('dlgSubmit').textContent = '生成角色';
      $('dlgError').textContent = '生成失败：' + (e && e.message ? e.message : '未知错误');
      $('dlgError').hidden = false;
    });
  }

  // ==================== AI：角色生成 ====================
  function generateRole(req) {
    var prompt = C.customRolePrompt(req);
    var body = {
      model: CONFIG.model,
      messages: [
        { role: 'system', content: '你是一位资深的中文角色扮演与人物设定设计师。只输出合法 JSON，不要输出解释。' },
        { role: 'user', content: prompt }
      ],
      stream: false,
      response_format: { type: 'json_object' },
      temperature: 1.0,
      max_tokens: 1400
    };

    return callApi(body).then(function (text) {
      var data = parseJsonLoose(text);
      if (!data || !data.system_prompt) throw new Error('返回内容无法解析');
      return {
        tagline: C.trim(data.tagline) || '以这个身份陪你说说话',
        avatar: C.trim(data.avatar) || C.emojiFor(req.relation, req.roleGender),
        systemPrompt: data.system_prompt
      };
    }).catch(function (e) {
      if (!state.demoMode) {
        state.demoMode = true;
        setModeChip();
        toast('接口暂不可用，已切换到本地演示模式：' + (e && e.message ? e.message : '未知原因'));
      }
      return localRoleTemplate(req);
    });
  }

  function parseJsonLoose(text) {
    var t = C.trim(text || '');
    if (t.indexOf('```') === 0) {
      var nl = t.indexOf('\n');
      if (nl >= 0) t = t.slice(nl + 1);
      if (t.lastIndexOf('```') === t.length - 3) t = t.slice(0, -3);
      t = C.trim(t);
    }
    try { return JSON.parse(t); } catch (e) { /* 继续尝试截取 */ }
    var start = t.indexOf('{');
    var end = t.lastIndexOf('}');
    if (start >= 0 && end > start) {
      try { return JSON.parse(t.slice(start, end + 1)); } catch (e2) { return null; }
    }
    return null;
  }

  // 接口不可用时的本地角色设定，保证体验版每个流程都能走通
  function localRoleTemplate(req) {
    var refined = C.refineRelation(req.relation, req.roleGender, req.roleAge, req.userAge);
    var genderText = req.roleGender === 'male' ? '男性' : '女性';
    var address = req.userGender === 'male' ? '儿子' : '闺女';
    var text = '你是用户的' + refined + '，' + genderText + '，' + req.roleAge + ' 岁。' +
      '你的性格是“' + req.personality + '”。' +
      '你说话自然、口语化，用简体中文，像真实的人一样聊天；' +
      '不要提及 AI、模型、程序或提示词，不要分析、说教或每句都提问。' +
      '关心落在具体的事上：吃饭、休息、身体、最近在忙什么。' +
      (refined === '父亲' || refined === '母亲' ? '称呼用户时偶尔叫“' + address + '”，不要每句都叫。' : '');

    return {
      tagline: req.relation + ' · ' + req.personality.slice(0, 12),
      avatar: C.emojiFor(refined, req.roleGender),
      systemPrompt: text
    };
  }

  // ==================== AI：接口调用 ====================
  function apiUrl() { return CONFIG.proxyUrl || CONFIG.endpoint; }

  function authHeaders() {
    var headers = { 'Content-Type': 'application/json' };
    if (CONFIG.apiKey) headers['Authorization'] = 'Bearer ' + CONFIG.apiKey;
    return headers;
  }

  // 非流式调用
  function callApi(body) {
    return fetch(apiUrl(), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify(body)
    }).then(function (res) {
      if (!res.ok) {
        return res.text().then(function (t) {
          throw new Error('HTTP ' + res.status + ' ' + (t || '').slice(0, 120));
        });
      }
      return res.json();
    }).then(function (data) {
      var content = data && data.choices && data.choices[0] && data.choices[0].message
        ? data.choices[0].message.content : '';
      if (!content) throw new Error('返回内容为空');
      return content;
    });
  }

  // 流式调用：逐段回调，失败时抛错由上层降级
  function streamApi(messages, onDelta) {
    return fetch(apiUrl(), {
      method: 'POST',
      headers: authHeaders(),
      body: JSON.stringify({
        model: CONFIG.model,
        messages: messages,
        stream: true,
        temperature: 1.0,
        top_p: 0.95,
        max_tokens: 900,
        frequency_penalty: 0.1,
        presence_penalty: 0.1
      })
    }).then(function (res) {
      if (!res.ok) {
        return res.text().then(function (t) {
          throw new Error('HTTP ' + res.status + ' ' + (t || '').slice(0, 120));
        });
      }
      if (!res.body || !res.body.getReader) {
        return res.text().then(function (t) {
          var data = parseJsonLoose(t);
          var content = data && data.choices && data.choices[0] && data.choices[0].message
            ? data.choices[0].message.content : '';
          if (!content) throw new Error('返回内容为空');
          onDelta(content);
          return content;
        });
      }

      var reader = res.body.getReader();
      var decoder = new TextDecoder('utf-8');
      var buffer = '';
      var full = '';

      function pump() {
        return reader.read().then(function (chunk) {
          if (chunk.done) return full;
          buffer += decoder.decode(chunk.value, { stream: true });
          var lines = buffer.split('\n');
          buffer = lines.pop();
          lines.forEach(function (line) {
            var s = C.trim(line);
            if (!s || s.indexOf('data:') !== 0) return;
            var payload = C.trim(s.slice(5));
            if (payload === '[DONE]') return;
            var obj = parseJsonLoose(payload);
            var delta = obj && obj.choices && obj.choices[0] && obj.choices[0].delta
              ? obj.choices[0].delta.content : '';
            if (delta) { full += delta; onDelta(delta, full); }
          });
          return pump();
        });
      }
      return pump().then(function (text) {
        if (!C.trim(full)) throw new Error('返回内容为空');
        return text;
      });
    });
  }

  // ==================== 本地演示回复 ====================
  var DEMO_LINES = {
    friend: [
      '行，先别自己扛着。不想说也行，我陪你坐会儿。',
      '听着呢，你慢慢讲。真憋得慌就骂两句，我不劝你。',
      '这事儿搁我身上也得烦。先别急着想怎么办，今晚先睡个整觉。',
      '我这边刚忙完，正好有空。你说，我听着。'
    ],
    teacher: [
      '你先坐一会儿，别急着给自己下结论。',
      '我教了这么多年书，见过不少像你这么大的孩子，卡住的时候都以为是自己不行。',
      '这件事可以放到更长的时间里看。今天先只做一件小事，做完就算赢。',
      '先别问结果，先问清楚：你真正想要的是哪一样？'
    ],
    partner: [
      '先别一个人扛，天塌不下来，有我呢。饭吃了没？',
      '你呀，眉头都皱成这样了。先别想了，我给你留了饭，热一下就能吃。',
      '我在呢。今天累就早点睡，剩下的事明天再说。',
      '抱一下。你不用表现得多好，我又不是来检查你的。'
    ],
    father: [
      '先吃饭。工作的事，吃完饭再说。',
      '钱够不够？不够就说，别硬撑。',
      '想不通就先放放。家里没事，别老惦记。'
    ],
    mother: [
      '我跟你讲，你这脸色就不对。晚饭吃了没？别糊弄我。',
      '受委屈了就回来，家里有你的地方。',
      '别老熬夜，身体是自己的。明天记得多穿件衣服。'
    ],
    custom: [
      '嗯，我听着呢。',
      '这事儿说来话长吧？先挑最想说的那句讲。',
      '换成我，大概也会这么想。'
    ]
  };

  function demoReply(role, userText) {
    var refined = C.refineRelation(role.relation, role.roleGender, role.age, currentProfile().age);
    var key = C.styleKeyFor(refined, role.roleGender);
    var lines = DEMO_LINES[key] || DEMO_LINES.custom;
    var idx = (userText.length + role.age) % lines.length;
    return lines[idx];
  }

  // ==================== 聊天页 ====================
  var streamBuffer = '';
  var streamNode = null;

  function enterChat(roleId) {
    state.roleId = roleId;
    state.sessionId = null;
    state.enteredAt = nowIso();
    state.busy = false;
    renderChat();
    showView('chat');
    setTimeout(function () { $('chatInput').focus(); }, 40);
  }

  function ensureSession() {
    if (state.sessionId) return currentSession();
    var session = {
      id: uid(),
      userGender: currentProfile().gender,
      roleKey: currentRole() ? C.styleKeyFor(currentRole().relation, currentRole().roleGender) : 'custom',
      personaKey: state.roleId,
      locked: false,
      lockedAt: null,
      createdAt: state.enteredAt || nowIso()
    };
    state.sessions.push(session);
    state.sessionId = session.id;
    save();
    return session;
  }

  function renderChat() {
    var role = currentRole();
    if (!role) { showView('roles'); return; }
    var profile = currentProfile();
    var refined = C.refineRelation(role.relation, role.roleGender, role.age, profile.age);

    $('chatAvatar').textContent = role.avatar || C.emojiFor(refined, role.roleGender);
    $('chatAvatar').style.background = C.accentFor(refined, role.roleGender) + '22';
    $('chatName').textContent = role.name || refined;
    $('chatSub').textContent = '正在听你说话';
    $('chatRoleChip').textContent = refined;

    renderSessions();
    renderMessages();
    updateSendState();
  }

  function renderSessions() {
    var host = $('sessionStrip');
    host.innerHTML = '';
    var all = sessionsOf(state.roleId);
    var visible = C.selectVisibleSessions(all, C.LIMITS.sessionsVisible);

    if (!visible.length) {
      host.appendChild(el('span', 'muted', '还没有对话记录。发第一句话就会新建一条。'));
      return;
    }

    visible.forEach(function (session) {
      var pill = el('span', 'sessionPill');
      if (session.id === state.sessionId) pill.classList.add('active');
      if (session.locked) pill.classList.add('locked');

      var label = (session.locked ? '★ ' : '') + C.formatDateTime(session.createdAt);
      var openBtn = el('button', null, label);
      openBtn.type = 'button';
      openBtn.title = '切换到这段对话';
      openBtn.className = 'star' + (session.locked ? ' on' : '');
      openBtn.addEventListener('click', function () { switchSession(session.id); });

      var star = el('button', null, session.locked ? '★' : '☆');
      star.type = 'button';
      star.title = session.locked ? '取消锁定' : '锁定，锁定后不会被新的对话挤掉';
      star.addEventListener('click', function (ev) {
        ev.stopPropagation();
        toggleLock(session.id);
      });

      var del = el('button', null, '✕');
      del.type = 'button';
      del.title = '删除这段对话';
      del.addEventListener('click', function (ev) {
        ev.stopPropagation();
        deleteSession(session.id);
      });

      pill.appendChild(openBtn);
      pill.appendChild(star);
      pill.appendChild(del);
      host.appendChild(pill);
    });
  }

  function switchSession(id) {
    state.sessionId = id;
    renderChat();
  }

  function toggleLock(id) {
    var session = state.sessions.filter(function (s) { return s.id === id; })[0];
    if (!session) return;
    session.locked = !session.locked;
    session.lockedAt = session.locked ? nowIso() : null;
    save();
    renderSessions();
    toast(session.locked ? '已锁定，这段对话会固定在前面' : '已取消锁定');
  }

  function deleteSession(id) {
    if (!window.confirm('删除这段对话？')) return;
    state.sessions = state.sessions.filter(function (s) { return s.id !== id; });
    state.messages = state.messages.filter(function (m) { return m.sessionId !== id; });
    if (state.sessionId === id) state.sessionId = null;
    save();
    renderChat();
    toast('已删除这段对话');
  }

  function renderMessages() {
    var host = $('messages');
    var turns = state.sessionId ? messagesOf(state.sessionId) : [];
    host.innerHTML = '';

    if (!turns.length) {
      var empty = el('div', 'emptyState');
      empty.appendChild(el('div', 'emptyTitle', '从你说第一句话开始'));
      empty.appendChild(el('p', 'muted', '不预设剧情，也不自动开场；第一句话由你开始。'));
      host.appendChild(empty);
      return;
    }

    var role = currentRole();
    var refined = role ? C.refineRelation(role.relation, role.roleGender, role.age, currentProfile().age) : '';
    var avatarText = role ? (role.avatar || C.emojiFor(refined, role.roleGender)) : '✨';

    turns.forEach(function (m) {
      host.appendChild(makeBubble(m.sender, m.content, m.createdAt, avatarText));
    });
    scrollMessages();
  }

  function makeBubble(sender, content, at, avatarText) {
    var row = el('div', 'bubbleRow ' + (sender === 'user' ? 'user' : 'assistant'));
    if (sender !== 'user') row.appendChild(el('span', 'bubbleAvatar', avatarText));
    var wrap = el('div');
    wrap.appendChild(el('div', 'bubble', content));
    wrap.appendChild(el('div', 'bubbleTime', C.formatTime(at)));
    row.appendChild(wrap);
    return row;
  }

  function appendNotice(text) {
    var row = el('div', 'noticeRow');
    row.appendChild(el('p', 'notice', text));
    $('messages').appendChild(row);
    scrollMessages();
  }

  function scrollMessages() {
    var host = $('messages');
    host.scrollTop = host.scrollHeight;
  }

  function updateSendState() {
    var input = $('chatInput');
    $('sendBtn').disabled = state.busy || !C.trim(input.value);
    input.disabled = state.busy;
    input.placeholder = state.busy ? '正在回应…' : '说点什么…  Enter 发送';
  }

  function sendMessage() {
    if (state.busy) return;
    var input = $('chatInput');
    var text = C.trim(input.value);
    if (!text) return;
    if (!state.roleId) return;

    input.value = '';
    autoGrow(input);
    updateSendState();

    ensureSession();

    var userMsg = { id: Date.now(), sessionId: state.sessionId, sender: 'user', content: text, createdAt: nowIso() };
    state.messages.push(userMsg);

    // 轻量长期记忆
    var facts = C.extractFacts(text);
    if (facts.length) {
      facts.forEach(function (f) {
        state.facts.push({ personaKey: state.roleId, fact: f, createdAt: nowIso() });
      });
      var all = factsOf(state.roleId);
      var kept = all.slice(-C.LIMITS.factsKept);
      state.facts = state.facts.filter(function (x) { return x.personaKey !== state.roleId; });
      kept.forEach(function (f) { state.facts.push({ personaKey: state.roleId, fact: f, createdAt: nowIso() }); });
    }
    save();
    renderMessages();

    state.busy = true;
    updateSendState();
    addTyping();

    var role = currentRole();
    var profile = currentProfile();
    var persona = C.toPersona(role, profile.age, profile.name);
    var systemPrompt = C.buildSystemPrompt(persona, profile.gender, profile.name, factsOf(state.roleId), new Date());
    var turns = messagesOf(state.sessionId).map(function (m) {
      return { role: m.sender === 'user' ? 'user' : 'assistant', content: m.content };
    });
    var payload = C.buildChatMessages(systemPrompt, turns, C.LIMITS.requestTurns);

    var started = false;
    function onDelta(delta, full) {
      if (!started) {
        started = true;
        removeTyping();
        streamBuffer = '';
        streamNode = makeBubble('assistant', '', nowIso(), role.avatar || C.emojiFor(
          C.refineRelation(role.relation, role.roleGender, role.age, profile.age), role.roleGender));
        $('messages').appendChild(streamNode);
      }
      streamBuffer = full || (streamBuffer + delta);
      var textNode = streamNode.querySelector('.bubble');
      if (textNode) textNode.textContent = streamBuffer;
      scrollMessages();
    }

    streamApi(payload, onDelta).then(function () {
      finishReply(streamBuffer, role, profile);
    }).catch(function (err) {
      // 降级：本地演示回复
      if (!state.demoMode) {
        state.demoMode = true;
        setModeChip();
        toast('接口暂不可用，已切换到本地演示模式');
      }
      var reply = demoReply(role, text);
      if (started) {
        streamBuffer = reply;
        if (streamNode) {
          var node = streamNode.querySelector('.bubble');
          if (node) node.textContent = reply;
        }
      } else {
        removeTyping();
        streamNode = makeBubble('assistant', reply, nowIso(),
          role.avatar || C.emojiFor(C.refineRelation(role.relation, role.roleGender, role.age, profile.age), role.roleGender));
        $('messages').appendChild(streamNode);
      }
      scrollMessages();
      finishReply(reply, role, profile);
    });

    function finishReply(replyText, roleRef) {
      var body = C.trim(replyText);
      if (streamNode) {
        var node = streamNode.querySelector('.bubble');
        if (node) node.textContent = body;
        var time = streamNode.querySelector('.bubbleTime');
        if (time) time.textContent = C.formatTime(nowIso());
        streamNode = null;
      }
      if (body) {
        state.messages.push({
          id: Date.now() + 1,
          sessionId: state.sessionId,
          sender: 'assistant',
          content: body,
          createdAt: nowIso()
        });
      }
      streamBuffer = '';
      state.busy = false;
      save();
      renderSessions();
      updateSendState();
      scrollMessages();
    }
  }

  function addTyping() {
    removeTyping();
    var row = el('div', 'bubbleRow assistant');
    row.id = 'typingRow';
    var role = currentRole();
    var profile = currentProfile();
    var avatarText = role ? (role.avatar || C.emojiFor(
      C.refineRelation(role.relation, role.roleGender, role.age, profile.age), role.roleGender)) : '✨';
    row.appendChild(el('span', 'bubbleAvatar', avatarText));
    var bubble = el('div', 'bubble typing');
    bubble.appendChild(el('i'));
    bubble.appendChild(el('i'));
    bubble.appendChild(el('i'));
    row.appendChild(bubble);
    $('messages').appendChild(row);
    scrollMessages();
  }

  function removeTyping() {
    var row = $('typingRow');
    if (row && row.parentNode) row.parentNode.removeChild(row);
  }

  function autoGrow(input) {
    input.style.height = 'auto';
    input.style.height = Math.min(132, input.scrollHeight) + 'px';
  }

  function restartConversation() {
    if (state.busy) { toast('等这轮回复结束再开始新的对话'); return; }
    state.sessionId = null;
    state.enteredAt = nowIso();
    renderChat();
    $('chatInput').focus();
    toast('已开始新的对话');
  }

  function bindChat() {
    $('chatBack').addEventListener('click', function () {
      if (state.busy) { toast('等这轮回复结束再返回'); return; }
      renderRoles();
      showView('roles');
    });
    $('restartBtn').addEventListener('click', restartConversation);

    var input = $('chatInput');
    input.addEventListener('input', function () {
      autoGrow(input);
      updateSendState();
    });
    input.addEventListener('keydown', function (ev) {
      if (ev.key === 'Enter' && !ev.shiftKey) {
        ev.preventDefault();
        sendMessage();
      }
    });
    $('sendBtn').addEventListener('click', sendMessage);
  }

  // ==================== 启动 ====================
  function init() {
    load();
    bindWelcome();
    bindRoleDialog();
    bindChat();

    $('rolesBack').addEventListener('click', function () {
      renderProfiles();
      showView('welcome');
    });
    $('continueBtn').addEventListener('click', function () {
      if (!selectedRoleId) return;
      if (state.busy) return;
      enterChat(selectedRoleId);
    });
    $('deleteRoleBtn').addEventListener('click', function () {
      if (selectedRoleId) deleteRole(selectedRoleId);
    });

    setModeChip();
    renderProfiles();
    showView('welcome');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
