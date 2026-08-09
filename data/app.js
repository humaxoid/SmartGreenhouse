'use strict';const LANG_KEY='gh_lang';const GH_I18N={en:{nav_control:'Dashboard',nav_settings:'Settings',nav_about:'About',btn_logout:'Logout',lbl_time:'TIME',lbl_uptime:'UPTIME',lbl_sensors:'Sensors',lbl_temp:'Temperature',lbl_hum:'Humidity',lbl_press:'Pressure',lux_unit:'lx',lbl_vents:'Vents',lbl_upper_travel:'Upper (42 sec)',lbl_lower_travel:'Lower (24 sec)',lbl_upper:'Upper vent',lbl_lower:'Lower vent',lbl_closed:'Closed',lbl_opened:'Open',btn_calibrate:'Calibrate',lbl_travel_sec:'Travel time, sec',btn_vent_open:'OPEN',btn_vent_close:'CLOSE',lbl_temp_open:'Open at, °C',lbl_temp_close:'Close at, °C',lbl_calibration:'Calibration (full travel time)',lbl_calib_top:'Upper, sec',lbl_calib_bottom:'Lower, sec',lbl_rain:'Rain',vstate_idle:'Idle',vstate_open_top:'Opening upper...',vstate_open_bottom:'Opening lower...',vstate_close_top:'Closing upper...',vstate_close_bottom:'Closing lower...',vstate_pause:'Pause (T measure)...',vstate_rain:'Rain — emergency close',lbl_vent_open_t:'Open at, \u00b0C',lbl_vent_close_t:'Close at, \u00b0C',lbl_irrigation:'Irrigation',lbl_irr1:'Irrigation 1',lbl_irr2:'Irrigation 2',lbl_irr3:'Irrigation 3',lbl_start:'Start time',lbl_dur:'Duration, min',lbl_interval:'Interval, h',timer_active:'ACTIVE',lbl_apply:'Apply',lbl_apply_irr:'\u2699 Apply timer settings',lbl_apply_hum:'Apply',lbl_apply_water:'Apply',lbl_humidifier:'Humidifier',lbl_state:'State',lbl_humid_on:'On',lbl_humid_off:'Off',lbl_hum_off_pct:'Off at humidity, %',lbl_hum_on_pct:'On at humidity, %',lbl_water_tank:'Water tank',lbl_pump_state:'Pump',lbl_pump_on:'On',lbl_pump_off:'Off',lbl_pump_fault:'Pump fault \u2014 check sensor',btn_reset_fault:'Reset',lbl_pump_on_pct:'On at, %',lbl_pump_off_pct:'Off at, %',lbl_tank_depth:'Depth, cm',lbl_pump_timeout:'Timeout, min',lbl_history:'History (24 h)',tab_temp:'Temperature',tab_hum:'Humidity',tab_press:'Pressure',tab_water:'Water level',chart_inside:'Inside (BME280)',chart_outside:'Outside (DHT22)',chart_empty:'Data accumulating... (updated every hour)',lbl_manual:'Manual',lbl_auto:'Auto',btn_on:'ON',btn_off:'OFF',set_tab_network:'Network',set_tab_mqtt:'MQTT',set_tab_creds:'Access',set_tab_ntp:'Time',set_tab_ui:'Interface',set_tab_ota:'Update',set_tab_system:'System',set_wifi:'Wi-Fi parameters',set_ssid:'SSID (network name)',set_wpass:'Wi-Fi password (empty = keep current)',set_wifi_scan:'Scan networks',set_forget_wifi:'Forget Wi-Fi',set_ip_mode:'IP mode',set_dhcp:'DHCP (auto)',set_static:'Static',set_dhcp_hint:'Controller receives address automatically from the router.',set_ip:'IP address',set_mask:'Subnet mask',set_gw:'Gateway',set_apply_net:'Apply & reboot',set_refresh:'Reload',set_reboot_warn:'\u26a0 Controller will reboot after apply (~10 sec)',set_mqtt_title:'MQTT (Home Assistant / Node-RED)',set_mqtt_enable:'Enable MQTT',set_mqtt_off:'no link',set_mqtt_disabled:'off',set_mqtt_on:'connected',set_mqtt_server:'MQTT server (IP or hostname)',set_mqtt_port:'Port',set_mqtt_user:'User (empty = no auth)',set_mqtt_pass:'MQTT password (empty = keep)',set_mqtt_apply:'Save MQTT',set_mqtt_resend:'Re-publish discovery',set_webaccess:'Web panel access',set_login:'Login',set_password:'Password',set_save_creds:'Save credentials',set_creds_warn:'\u26a0 You will need to re-login after change',set_ntp_title:'Time synchronization (NTP)',set_ntp_server:'NTP server address',set_ntp_tz:'Timezone',set_ntp_interval:'Sync interval',set_ntp_apply:'Save NTP',set_ui_title:'Interface',set_theme:'Color theme',set_lang:'Interface language',theme_green:'\ud83c\udf40 Green',theme_beige:'\ud83c\udf3e Beige',theme_night:'\ud83c\udf19 Night',theme_win98:'\ud83d\udcbb Light gray',theme_blue:'\u2600 Sky blue',theme_brown:'\ud83c\udf42 Brown',set_ota_title:'Firmware update',set_ota_file:'File',set_ota_choose:'Choose file',set_ota_no_file:'No file chosen',set_ota_upload:'Upload',set_ota_current:'Current version:',ota_detected_container:'\u2713 Container {v} detected',ota_hint:'Upload an update file like greenhouse_vX.Y.bin. The controller will reboot automatically after update.',ota_detected_fw:'\u2713 Firmware update detected',ota_detected_fs:'\u2713 Filesystem update detected',ota_bad_name:'\u2717 Invalid filename. Use "greenhouse_vX.Y.bin" only.',ota_uploading:'Uploading...',ota_done:'Update complete. Rebooting...',ota_failed:'Update failed',set_sys_title:'System operations',set_sys_reboot:'Reboot ESP32',set_sys_clear_hist:'Clear history',set_sys_factory:'Factory reset',set_sys_info:'Information',info_fw:'Firmware:',info_ip:'IP address:',info_mac:'MAC:',info_uptime:'Uptime:',info_heap:'Free heap:',info_rssi:'Wi-Fi RSSI:',about_title:'Smart Greenhouse',about_version:'Version',about_desc:'Greenhouse automation: irrigation, ventilation, humidification, water tank.<br>Real-time monitoring (BME280, DHT22, BH1750, AJ-SR04M).',about_author:'\u00a9 Sergey Saidov',offline_title:'Controller unavailable',offline_sub:'No connection to ESP32. Page loaded from cache.<br>Check controller power.',offline_btn:'\u21bb Reload page',login_title:'Sign in',login_user:'Login',login_pass:'Password',login_btn:'Sign in',ws_connected:'Connected',ws_connecting:'Connecting...',ws_offline:'No connection',ws_auth_fail:'Invalid credentials',toast_saved:'Saved',toast_no_conn:'No connection',toast_confirm_logout:'Logout?',toast_confirm_reboot:'Reboot ESP32?',toast_confirm_clear:'Clear all history?',toast_confirm_factory:'FACTORY RESET. All settings will be lost. Continue?',toast_confirm_factory2:'Are you sure? This cannot be undone.',toast_confirm_forget_wifi:'Delete Wi-Fi credentials and reboot into AP mode?',toast_confirm_calibrate:'Run vent calibration? The vent will close completely.',toast_calib_started:'Calibration started',toast_fault_reset:'Fault reset',toast_discovery_sent:'Discovery published',lbl_homing:'Position reset',lbl_homing_hint:'Closes the sash to the limit switch and resets the counter to zero. Use if the shown position no longer matches reality.',btn_home_top:'Reset upper',btn_home_bottom:'Reset lower',lbl_homing_run:'Running\u2026',btn_home_cancel:'Stop',toast_confirm_home:'The sash will close completely and its position counter will reset to zero. Continue?',toast_home_started:'Position reset started',err_fill_all:'Fill in all fields',err_close_lt_open:'\u201cClose\u201d must be below \u201cOpen\u201d',err_calib_range:'Travel time: 1\u2013120 s',msg_no_changes:'Nothing changed',err_bad_thresholds:'Check the thresholds',err_on_lt_off:'\u201cOn\u201d must be below \u201cOff\u201d',err_creds_len:'Login from 1 character, password from 4',err_generic:'Error',msg_rebooting:'Rebooting\u2026',msg_resetting:'Resetting to factory defaults\u2026',err_load_settings:'Could not load settings',err_load_network:'Could not load network settings',err_load_mqtt:'Could not load MQTT settings',wifi_scanning:'Scanning\u2026',wifi_none:'No networks found',wifi_scan_failed:'Scan failed',},ru:{nav_control:'Управление',nav_settings:'Настройки',nav_about:'О программе',btn_logout:'Выход',lbl_time:'ВРЕМЯ',lbl_uptime:'UPTIME',lbl_sensors:'Датчики',lbl_temp:'Температура',lbl_hum:'Влажность',lbl_press:'Давление',lux_unit:'лк',lbl_vents:'Форточки',lbl_upper_travel:'Верхняя (42 сек)',lbl_lower_travel:'Нижняя (24 сек)',lbl_upper:'Верхняя форточка',lbl_lower:'Нижняя форточка',lbl_closed:'Закрыта',lbl_opened:'Открыта',btn_calibrate:'Калибровка',lbl_travel_sec:'Время хода, сек',btn_vent_open:'ОТКРЫТЬ',btn_vent_close:'ЗАКРЫТЬ',lbl_temp_open:'Открыть при, °C',lbl_temp_close:'Закрыть при, °C',lbl_calibration:'Калибровка (полное время хода)',lbl_calib_top:'Верхняя, сек',lbl_calib_bottom:'Нижняя, сек',lbl_rain:'Дождь',vstate_idle:'Ожидание',vstate_open_top:'Открывается верхняя...',vstate_open_bottom:'Открывается нижняя...',vstate_close_top:'Закрывается верхняя...',vstate_close_bottom:'Закрывается нижняя...',vstate_pause:'Пауза (замер t)...',vstate_rain:'Дождь — экстренное закрытие',lbl_vent_open_t:'Открыть при, \u00b0C',lbl_vent_close_t:'Закрыть при, \u00b0C',lbl_irrigation:'Полив',lbl_irr1:'Полив 1',lbl_irr2:'Полив 2',lbl_irr3:'Полив 3',lbl_start:'Время старта',lbl_dur:'Длит., мин',lbl_interval:'Интервал, ч',timer_active:'АКТИВИРОВАН',lbl_apply:'Применить',lbl_apply_irr:'\u2699 Применить настройки таймеров',lbl_apply_hum:'Применить',lbl_apply_water:'Применить',lbl_humidifier:'Увлажнитель',lbl_state:'Состояние',lbl_humid_on:'Включён',lbl_humid_off:'Отключён',lbl_hum_off_pct:'Выкл при влажн., %',lbl_hum_on_pct:'Вкл при влажн., %',lbl_water_tank:'Накопительная ёмкость',lbl_pump_state:'Насос',lbl_pump_on:'Включён',lbl_pump_off:'Выключен',lbl_pump_fault:'Авария насоса \u2014 проверьте датчик',btn_reset_fault:'Сбросить',lbl_pump_on_pct:'Вкл при, %',lbl_pump_off_pct:'Выкл при, %',lbl_tank_depth:'Глубина, см',lbl_pump_timeout:'Таймаут, мин',lbl_history:'История (24 ч)',tab_temp:'Температура',tab_hum:'Влажность',tab_press:'Давление',tab_water:'Уровень воды',chart_inside:'Внутри (BME280)',chart_outside:'Снаружи (DHT22)',chart_empty:'Данные накапливаются... (обновление каждый час)',lbl_manual:'Ручной',lbl_auto:'Авто',btn_on:'ВКЛ',btn_off:'ВЫКЛ',set_tab_network:'Сеть',set_tab_mqtt:'MQTT',set_tab_creds:'Доступ',set_tab_ntp:'Время',set_tab_ui:'Интерфейс',set_tab_ota:'Обновление',set_tab_system:'Система',set_wifi:'Параметры WiFi',set_ssid:'SSID (имя сети)',set_wpass:'Пароль WiFi (пусто = без изменений)',set_wifi_scan:'Сканировать сети',set_forget_wifi:'Забыть Wi-Fi',set_ip_mode:'Режим IP-адреса',set_dhcp:'DHCP (авто)',set_static:'Статический',set_dhcp_hint:'Контроллер получает адрес автоматически от роутера.',set_ip:'IP-адрес',set_mask:'Маска',set_gw:'Шлюз',set_apply_net:'Применить и перезагрузить',set_refresh:'Обновить',set_reboot_warn:'\u26a0 После применения контроллер перезагрузится (~10 сек)',set_mqtt_title:'MQTT (Home Assistant / Node-RED)',set_mqtt_enable:'Включить MQTT',set_mqtt_off:'нет связи',set_mqtt_disabled:'отключён',set_mqtt_on:'подключён',set_mqtt_server:'MQTT сервер (IP или hostname)',set_mqtt_port:'Порт',set_mqtt_user:'Логин (пусто = без авторизации)',set_mqtt_pass:'Пароль MQTT (пусто = без изменений)',set_mqtt_apply:'Сохранить MQTT',set_mqtt_resend:'Пересоздать discovery',set_webaccess:'Доступ к веб-панели',set_login:'Логин',set_password:'Пароль',set_save_creds:'Сохранить доступ',set_creds_warn:'\u26a0 После смены потребуется повторный вход',set_ntp_title:'Синхронизация времени (NTP)',set_ntp_server:'Адрес NTP-сервера',set_ntp_tz:'Часовой пояс',set_ntp_interval:'Интервал синхронизации',set_ntp_apply:'Сохранить NTP',set_ui_title:'Интерфейс',set_theme:'Схема оформления',set_lang:'Язык интерфейса',theme_green:'\ud83c\udf40 Зелёная',theme_beige:'\ud83c\udf3e Бежевая',theme_night:'\ud83c\udf19 Ночная',theme_win98:'\ud83d\udcbb Светло-серая',theme_blue:'\u2600 Бледно-голубая',theme_brown:'\ud83c\udf42 Коричневая',set_ota_title:'Обновление прошивки',set_ota_file:'Файл',set_ota_choose:'Выбрать файл',set_ota_no_file:'Файл не выбран',set_ota_upload:'Загрузить',set_ota_current:'Текущая версия:',ota_detected_container:'\u2713 Контейнер {v} обнаружен',ota_hint:'Загрузите файл обновления вида greenhouse_vX.Y.bin. Контроллер автоматически перезагрузится после обновления.',ota_detected_fw:'\u2713 Обнаружено обновление прошивки',ota_detected_fs:'\u2713 Обнаружено обновление файловой системы',ota_bad_name:'\u2717 Недопустимое имя файла. Используйте только "greenhouse_vX.Y.bin".',ota_uploading:'Загрузка...',ota_done:'Обновление завершено. Перезагрузка...',ota_failed:'Ошибка обновления',set_sys_title:'Системные операции',set_sys_reboot:'Перезагрузить ESP32',set_sys_clear_hist:'Очистить историю',set_sys_factory:'Сброс к заводским настройкам',set_sys_info:'Информация',info_fw:'Прошивка:',info_ip:'IP адрес:',info_mac:'MAC:',info_uptime:'Uptime:',info_heap:'Free heap:',info_rssi:'Wi-Fi RSSI:',about_title:'Умная теплица',about_version:'Версия',about_desc:'Система автоматизации теплицы: полив, вентиляция, увлажнение, накопительная ёмкость.<br>Мониторинг в реальном времени (BME280, DHT22, BH1750, AJ-SR04M).',about_author:'\u00a9 Сергей Саидов',offline_title:'Контроллер недоступен',offline_sub:'Нет соединения с ESP32. Страница загружена из кэша.<br>Проверьте питание контроллера.',offline_btn:'\u21bb Обновить страницу',login_title:'Вход в систему',login_user:'Логин',login_pass:'Пароль',login_btn:'Войти',ws_connected:'Подключено',ws_connecting:'Подключение...',ws_offline:'Нет соединения',ws_auth_fail:'Неверный логин или пароль',toast_saved:'Сохранено',toast_no_conn:'Нет соединения',toast_confirm_logout:'Выйти?',toast_confirm_reboot:'Перезагрузить ESP32?',toast_confirm_clear:'Очистить всю историю?',toast_confirm_factory:'СБРОС К ЗАВОДСКИМ. Все настройки будут потеряны. Продолжить?',toast_confirm_factory2:'Точно? Это действие нельзя отменить.',toast_confirm_forget_wifi:'Удалить Wi-Fi креды и перезагрузиться в режим AP?',toast_confirm_calibrate:'Запустить калибровку форточки? Форточка полностью закроется.',toast_calib_started:'Калибровка запущена',toast_fault_reset:'Авария сброшена',toast_discovery_sent:'Discovery отправлен',lbl_homing:'Сброс позиции',lbl_homing_hint:'Закрывает створку до концевика и обнуляет счётчик. Применяйте, если показанная позиция разошлась с реальностью.',btn_home_top:'Сбросить верхнюю',btn_home_bottom:'Сбросить нижнюю',lbl_homing_run:'Идёт прогон…',btn_home_cancel:'Остановить',toast_confirm_home:'Створка полностью закроется, а счётчик позиции обнулится. Продолжить?',toast_home_started:'Сброс позиции запущен',err_fill_all:'Заполните все поля',err_close_lt_open:'«Закрыть» должно быть меньше «Открыть»',err_calib_range:'Время хода: 1–120 с',msg_no_changes:'Изменений нет',err_bad_thresholds:'Проверьте пороги',err_on_lt_off:'«Вкл» должно быть меньше «Выкл»',err_creds_len:'Логин от 1 символа, пароль от 4',err_generic:'Ошибка',msg_rebooting:'Перезагрузка…',msg_resetting:'Сброс к заводским настройкам…',err_load_settings:'Не удалось загрузить настройки',err_load_network:'Не удалось загрузить настройки сети',err_load_mqtt:'Не удалось загрузить настройки MQTT',wifi_scanning:'Поиск сетей…',wifi_none:'Сети не найдены',wifi_scan_failed:'Не удалось выполнить поиск',}};let currentLang='en';
// Кеш последних значений формы форточек — для отправки только изменённых полей
var _lastFormVent={vTOpen:null,vTClose:null,vuTravel:null,vlTravel:null};function i18n(k){var d=GH_I18N[currentLang]||GH_I18N.en;var v=d[k];if(v==null)v=GH_I18N.en[k];return v!=null?v:k;}
const WS_URL='ws://'+location.hostname+'/ws';const RECONNECT_MS=3000,OFFLINE_AFTER_N=4;var _ss=window.sessionStorage;var _authUser=_ss.getItem('gh_u')||'';var _authPass=_ss.getItem('gh_p')||'';let ws=null,wsTimer=null,wsReconnectCount=0,statePoll=null,_wsAuthed=false;let histData=[],activeChart='temp',_btnLastMs={};const $=function(id){return document.getElementById(id);};const pad2=function(n){return String(n).padStart(2,'0');};function setVal(id,v){var e=$(id);if(e!=null&&v!==undefined&&v!==null)e.value=v;}
// v6.1: перевод RSSI в проценты.
//
// Проценты у Wi-Fi — придуманная для человека абстракция: dBm величина
// логарифмическая, и «процент мощности» дал бы бессмыслицу. Поэтому важно
// не «правильно посчитать», а выбрать шкалу, по которой видно, когда пора
// что-то делать с антенной.
//
// Линейная формула Windows (2*(rssi+100)) отвергнута: она даёт 40 % на
// -80 dBm, хотя это уже граница работоспособности, и расходится с порогами
// окраски текста рядом (-65 / -80), которые в проекте были и раньше.
//
// Кусочная кривая ниже привязана к общепринятым порогам качества связи:
//   -50 и выше  отлично      100 %
//   -60         очень хорошо  80 %
//   -70         приемлемо     50 %
//   -80         плохо         25 %
//   -90         почти нет      5 %
//   -100        нет            0 %
//
// RSSI == 0 библиотека Wi-Fi отдаёт при отсутствии соединения — трактуем
// как «нет сигнала», иначе ноль дБм читался бы как идеальный уровень.
function rssiToPct(rssi){
  if(!rssi||rssi>=0)return 0;
  var pts=[[-50,100],[-60,80],[-70,50],[-80,25],[-90,5],[-100,0]];
  if(rssi>=pts[0][0])return 100;
  for(var i=0;i<pts.length-1;i++){
    var a=pts[i],b=pts[i+1];
    if(rssi<=a[0]&&rssi>=b[0]){
      var k=(rssi-a[0])/(b[0]-a[0]);
      return Math.round(a[1]+k*(b[1]-a[1]));
    }
  }
  return 0;
}
function rssiColor(pct){
  return 'hsl('+Math.round(Math.max(0,Math.min(100,pct))*1.2)+',72%,45%)';
}
function toast(msg,type){var c=$('toastContainer');if(!c)return;var el=document.createElement('div');el.className='toast '+(type||'info');el.innerHTML=msg;c.appendChild(el);setTimeout(function(){el.remove();},4000);}
function showLoginScreen(){var ls=$('loginScreen'),as=$('appShell');if(ls)ls.style.display='flex';if(as)as.style.display='none';}
function showAppShell(){var ls=$('loginScreen'),as=$('appShell');if(ls)ls.style.display='none';if(as)as.style.display='flex';}
function doLogin(){var u=($('loginUser').value||'').trim();var p=$('loginPass').value||'';var err=$('loginError');if(u.length<1||p.length<1){if(err){err.textContent=i18n('login_user')+' / '+i18n('login_pass');err.style.display='block';}
return;}
if(err)err.style.display='none';var btn=$('btnLogin');if(btn)btn.disabled=true;var b64=btoa(u+':'+p);fetch('/api/settings',{headers:{'Authorization':'Basic '+b64}}).then(function(r){if(btn)btn.disabled=false;if(r.ok){_authUser=u;_authPass=p;_ss.setItem('gh_u',u);_ss.setItem('gh_p',p);showAppShell();wsConnect();loadAllSettings();}else{if(err){err.textContent=i18n('ws_auth_fail');err.style.display='block';}
$('loginPass').value='';$('loginPass').focus();}}).catch(function(){if(btn)btn.disabled=false;if(err){err.textContent=i18n('toast_no_conn');err.style.display='block';}});}
function loginKeydown(e){if(e.key==='Enter')doLogin();}
function doLogout(){if(!confirm(i18n('toast_confirm_logout')))return;if(ws&&ws.readyState===WebSocket.OPEN)ws.close();_authUser='';_authPass='';_ss.removeItem('gh_u');_ss.removeItem('gh_p');showLoginScreen();}
window.doLogout=doLogout;function authHeaders(extra){var h=Object.assign({'Authorization':'Basic '+btoa(_authUser+':'+_authPass)},extra||{});return h;}
// v6.1: отметка выбранной плитки темы. aria-checked одновременно
// и состояние для чтения с экрана, и селектор подсветки в CSS.
function markThemeTile(theme){var w=$('themeTiles');if(!w)return;
w.querySelectorAll('.theme-tile').forEach(function(b){
b.setAttribute('aria-checked',b.dataset.themeVal===(theme||'')?'true':'false');});}
// Группа объявлена как radiogroup, значит обязана слушать стрелки:
// Tab выводит из группы, перемещение внутри — стрелками, как в любом
// наборе радиокнопок.
function setupThemeTiles(){var w=$('themeTiles');if(!w)return;
w.addEventListener('keydown',function(e){
var d=(e.key==='ArrowRight'||e.key==='ArrowDown')?1:(e.key==='ArrowLeft'||e.key==='ArrowUp')?-1:0;
if(!d)return;e.preventDefault();
var t=[].slice.call(w.querySelectorAll('.theme-tile'));
var i=t.indexOf(document.activeElement);if(i<0)i=0;
var n=t[(i+d+t.length)%t.length];n.focus();applyThemeAndSave(n.dataset.themeVal);});}
function applyTheme(theme){document.documentElement.dataset.theme=theme||'';markThemeTile(theme||'');if(histData&&histData.length)drawHistChart(activeChart);if(typeof redrawAll==='function')redrawAll();}
window.applyTheme=applyTheme;async function applyThemeAndSave(theme){applyTheme(theme);try{localStorage.setItem('gh_theme',theme||'');}catch(e){}
if(_authUser){try{await fetchJson('/api/ui',{method:'POST',headers:authHeaders({'Content-Type':'application/json'}),body:JSON.stringify({theme:theme||'',lang:currentLang})});}catch(e){console.warn('[UI] theme save failed',e);}}}
window.applyThemeAndSave=applyThemeAndSave;function applyLang(lang){if(!GH_I18N[lang])lang='en';currentLang=lang;document.documentElement.lang=lang;document.querySelectorAll('[data-i18n]').forEach(function(el){var k=el.getAttribute('data-i18n');var v=i18n(k);if(v==null)return;if(k==='set_ota_note'||k==='about_desc'||k==='offline_sub'||k==='ota_step1'||k==='ota_step2'||k==='ota_step3'||k==='ota_step4')
el.innerHTML=v;else
el.textContent=v;});var langSel=$('langSel');if(langSel)langSel.value=lang;document.querySelectorAll('.relay-btn').forEach(function(b){b.textContent=b.classList.contains('on')?i18n('btn_on'):i18n('btn_off');});}
async function applyLangAndSave(lang){applyLang(lang);try{localStorage.setItem(LANG_KEY,lang);}catch(e){}
if(_authUser){try{var theme=document.documentElement.dataset.theme||'';await fetchJson('/api/ui',{method:'POST',headers:authHeaders({'Content-Type':'application/json'}),body:JSON.stringify({theme:theme,lang:lang})});}catch(e){console.warn('[UI] lang save failed',e);}}}
window.applyLang=applyLang;window.applyLangAndSave=applyLangAndSave;function showPage(name){var d=$('pageDashboard'),s=$('pageSettings');if(!d||!s)return;d.classList.remove('active');s.classList.remove('active');d.style.display='none';s.style.display='none';if(name==='settings'){s.classList.add('active');s.style.display='block';loadAllSettings();setTimeout(updateTabsHint,0);}else{d.classList.add('active');d.style.display='grid';setTimeout(redrawAll,50);}
document.querySelectorAll('.sb-link[data-page]').forEach(function(a){a.classList.toggle('sb-active',a.dataset.page===name);});closeSidebar();}
window.showPage=showPage;function showSettingsTab(tab){document.querySelectorAll('.set-tab').forEach(function(b){b.classList.remove('active');});var matched=null;document.querySelectorAll('.set-tab').forEach(function(b){if(b.getAttribute('onclick')&&b.getAttribute('onclick').indexOf("'"+tab+"'")>=0)matched=b;});if(matched)matched.classList.add('active');document.querySelectorAll('.set-pane').forEach(function(p){p.classList.toggle('active',p.dataset.pane===tab);});if(matched&&matched.scrollIntoView)matched.scrollIntoView({block:'nearest',inline:'nearest'});updateTabsHint();}
window.showSettingsTab=showSettingsTab;
// v6.1: подсказка прокрутки вкладок настроек. Класс вешается только
// на ту сторону, куда прокрутка реально возможна, — иначе стрелка
// обещает содержимое, которого нет.
function updateTabsHint(){var w=$('setTabsWrap'),n=$('setTabs');if(!w||!n)return;
var max=n.scrollWidth-n.clientWidth;
w.classList.toggle('can-left',n.scrollLeft>2);
w.classList.toggle('can-right',max>2&&n.scrollLeft<max-2);}
function setupTabsHint(){var n=$('setTabs');if(!n)return;
n.addEventListener('scroll',updateTabsHint,{passive:true});
window.addEventListener('resize',updateTabsHint);updateTabsHint();}
function toggleSidebar(){var sb=$('sidebar'),ov=$('sbOverlay');var o=sb&&sb.classList.toggle('open');if(ov)ov.classList.toggle('open',!!o);}
function closeSidebar(){[$('sidebar'),$('sbOverlay')].forEach(function(e){if(e)e.classList.remove('open');});}
window.toggleSidebar=toggleSidebar;window.closeSidebar=closeSidebar;function showOfflineOverlay(){var o=$('offlineOverlay');if(o)o.classList.add('show');}
function hideOfflineOverlay(){var o=$('offlineOverlay');if(o)o.classList.remove('show');}
function togglePass(id){var i=$(id);if(i)i.type=(i.type==='password')?'text':'password';}
window.togglePass=togglePass;// v6.1: учётные данные больше НЕ кладём в URL сокета — браузеры их
// всё равно не отправляют, толку от этого не было. Авторизация
// теперь протокольная: первым сообщением уходит cmd:'auth'.
function wsConnect(){if(ws&&ws.readyState!==WebSocket.CLOSED)return;
if(!_authUser||!_authPass)return;   // нечем авторизоваться

try{ws=new WebSocket(WS_URL);}catch(e){console.error('[WS] construct failed',e);wsTimer=setTimeout(wsConnect,RECONNECT_MS);return;}
ws.onopen=function(){clearTimeout(wsTimer);wsReconnectCount=0;hideOfflineOverlay();
// До подтверждения авторизации контроллер не отдаёт ни состояния,
// ни истории, поэтому опрос запускаем только по ответу на auth.
setWsUI('authing',i18n('ws_connecting'));
_wsAuthed=false;
ws.send(JSON.stringify({cmd:'auth',token:btoa(_authUser+':'+_authPass)}));};ws.onclose=function(){_wsAuthed=false;stopStatePoll();setWsUI('offline',i18n('ws_offline'));wsReconnectCount++;if(wsReconnectCount>=OFFLINE_AFTER_N)showOfflineOverlay();// v6.1: переподключаемся ТОЛЬКО если есть с чем авторизоваться.
// Иначе после отказа авторизации получался бесконечный цикл: креды
// очищены, пользователь на экране входа, а сокет каждые 3 секунды
// открывался заново и слал заведомо неверный auth.
if(_authUser&&_authPass)wsTimer=setTimeout(wsConnect,RECONNECT_MS);};ws.onerror=function(e){console.error('[WS]',e);};ws.onmessage=function(e){try{onMessage(JSON.parse(e.data));}catch(err){console.error('[WS parse]',err);}};}
// v6.1: контроллер больше не рассылает состояние сам (AsyncWebSocket не
// потокобезопасен — см. ws_handler.cpp). Панель сама опрашивает раз в
// секунду; ответ формируется в задаче AsyncTCP, гонки нет.
// Опрос идёт мимо wsSend, чтобы не мигать индикатором активности.
function startStatePoll(){stopStatePoll();statePoll=setInterval(function(){if(ws&&ws.readyState===WebSocket.OPEN&&_wsAuthed)ws.send(JSON.stringify({cmd:'get_state'}));},1000);}
function stopStatePoll(){if(statePoll){clearInterval(statePoll);statePoll=null;}}
function wsSend(o){if(!ws||ws.readyState!==WebSocket.OPEN||!_wsAuthed){toast(i18n('toast_no_conn'),'error');return false;}
ws.send(JSON.stringify(o));var sp=$('wsSpinner');if(sp){sp.classList.add('show');setTimeout(function(){sp.classList.remove('show');},1200);}
return true;}
function setWsUI(cls,label){var d=$('wsStatus'),l=$('wsLabel');if(d){d.className='ws-dot '+cls;d.title=label;}
if(l){l.textContent=label;l.classList.remove('blink-offline');if(cls==='online'){l.style.color='#00e676';}
else if(cls==='offline'){l.style.color='#f44336';l.classList.add('blink-offline');}
else{l.style.color='';}}}
function onMessage(m){if(m.type==='state')renderState(m);else if(m.type==='history')renderHistory(m.points);else if(m.type==='ack'){
  if(m.cmd==='auth'){
    if(m.ok){_wsAuthed=true;setWsUI('online',i18n('ws_connected'));startStatePoll();
      setTimeout(function(){wsSend({cmd:'get_history'});},600);}
    else{_wsAuthed=false;setWsUI('offline',i18n('ws_auth_fail'));onAuthLost();}
    return;}
  if(!m.ok)toast(i18n('err_generic')+': '+(m.error||m.cmd),'error');}}
function renderState(s){if(s.fwVersion)setFwVersion(s.fwVersion);
if(s.time){var tp=s.time.split(':');var eH=$('tH'),eM=$('tM');if(eH)eH.textContent=tp[0]||'--';if(eM)eM.textContent=tp[1]||'--';}
if(s.uptime){var sec=s.uptime|0;var d=Math.floor(sec/86400),h=Math.floor((sec%86400)/3600),m=Math.floor((sec%3600)/60);var eD=$('utD'),eH2=$('utH'),eM2=$('utM');if(eD)eD.textContent=pad2(d);if(eH2)eH2.textContent=pad2(h);if(eM2)eM2.textContent=pad2(m);$('infoUptime')&&($('infoUptime').textContent=d+'d '+pad2(h)+':'+pad2(m));}
if(s.wifi){var rssi=s.wifi.rssi|0;var pct=rssiToPct(rssi);var noLink=(pct===0&&rssi>=0);
var rf=$('sbRssiFill'),rp=$('sbRssiPct'),rd=$('sbRssiDbm'),rb=$('sbRssi');
if(rf){rf.style.width=pct+'%';rf.style.backgroundColor=rssiColor(pct);}
if(rp)rp.textContent=pct+'%';
if(rd)rd.textContent=noLink?'-- dBm':(rssi+' dBm');
if(rb)rb.className='sb-rssi '+(noLink?'error':rssi>-65?'ok':rssi>-80?'warn':'error');
$('infoRssi')&&($('infoRssi').textContent=rssi+' dBm');$('infoIp')&&($('infoIp').textContent=s.wifi.ip||'—');}
if(s.heap){var kb=Math.round(s.heap/1024);$('sbHeap')&&($('sbHeap').textContent='\ud83d\udcbe '+kb+' KB');$('infoHeap')&&($('infoHeap').textContent=kb+' KB');}
var sn=s.sensors||{};if(typeof sn.temp==='number'){animateTo(GAUGES[0],sn.temp,700);updateGaugeDom(GAUGES[0],sn.temp);}
if(typeof sn.humidity==='number'){animateTo(GAUGES[1],sn.humidity,700);updateGaugeDom(GAUGES[1],sn.humidity);}
if(typeof sn.pressure==='number'){animateTo(GAUGES[2],sn.pressure,700);updateGaugeDom(GAUGES[2],sn.pressure);}
if(sn.lux!==undefined)$('luxVal').textContent=sn.lux;else $('luxVal').textContent='---';
// v6.1: блок DHT22 показывает НАРУЖНЫЕ значения (tempOut/humidityOut).
// Раньше сюда подставлялись sn.temp/sn.humidity — объединённые значения
// с приоритетом BME280, из-за чего блок дублировал стрелочные приборы.
$('dhtT').textContent=(typeof sn.tempOut==='number')?sn.tempOut.toFixed(1):'--.-';
$('dhtH').textContent=(typeof sn.humidityOut==='number')?sn.humidityOut.toFixed(1):'--.-';
window._isRaining=!!sn.rain;updateLuxGif(sn.lux||0,sn.lux!==undefined,window._isRaining);if(sn.bmeOk!==undefined){var chips=[{lbl:'BME280',ok:sn.bmeOk||typeof sn.temp==='number'},{lbl:'DHT22',ok:sn.dhtOk},{lbl:'BH1750',ok:sn.luxOk},{lbl:'AJ-SR04M',ok:sn.waterOk}];$('sensorStatusRow').innerHTML=chips.map(function(c){return'<span class="sensor-chip '+(c.ok?'ok':'err')+'">'+(c.ok?'\u2713':'\u26a0')+' '+c.lbl+'</span>';}).join('');}
if(typeof sn.waterLevel==='number'){var p=Math.max(0,Math.min(100,sn.waterLevel));$('waterBarFill').style.height=p.toFixed(1)+'%';$('waterBarLabel').textContent=Math.round(p)+'%';}else{$('waterBarFill').style.height='0%';$('waterBarLabel').textContent='--%';}
if(s.relays){s.relays.forEach(function(state,idx){var btn=$('rBtn'+idx);if(btn){btn.classList.toggle('on',state);btn.classList.toggle('off',!state);btn.textContent=state?i18n('btn_on'):i18n('btn_off');}
if(idx===7){var hb=$('humidBadge');if(hb){hb.textContent=state?i18n('lbl_humid_on'):i18n('lbl_humid_off');hb.className='vent-badge '+(state?'open':'closed');}}
if(idx===8){var pb=$('pumpBadge');if(pb){pb.textContent=state?i18n('lbl_pump_on'):i18n('lbl_pump_off');pb.className='vent-badge '+(state?'open':'closed');}}});}
if(s.modes){applyModeVents(s.modes.vents===1);applyModeTimers(s.modes.timers===1);applyModeHumid(s.modes.humid===1);applyModePump(s.modes.pump===1);}
if(s.vents){updateVent(0,s.vents.upper);updateVent(1,s.vents.lower);var hom=!!((s.vents.upper&&s.vents.upper.calibrating)||(s.vents.lower&&s.vents.lower.calibrating));var hs=$('homingStatus');if(hs)hs.style.display=hom?'block':'none';['btnHomeTop','btnHomeBottom'].forEach(function(id){var b=$(id);if(b)b.disabled=hom;});var stEl=$('ventAutoState');if(stEl&&s.vents.autoState!==undefined){var stKeys=['vstate_idle','vstate_open_top','vstate_open_bottom','vstate_close_top','vstate_close_bottom','vstate_pause','vstate_rain'];var k=stKeys[s.vents.autoState];stEl.textContent=k?i18n(k):'?';}var rb=$('ventRainBadge');if(rb)rb.style.display=s.vents.rainBlocked?'inline-block':'none';}
if(s.timers){s.timers.forEach(function(t,i){onTimerState(i,t.active,t.remaining,t.nextStart);var fS=$('start'+i),fD=$('dur'+i),fI=$('int'+i);if(t.startMin!==undefined&&document.activeElement!==fS){var hh=Math.floor(t.startMin/60),mm=t.startMin%60;setVal('start'+i,pad2(hh)+':'+pad2(mm));}if(t.durationMin!==undefined&&document.activeElement!==fD)setVal('dur'+i,t.durationMin);if(t.intervalHours!==undefined&&document.activeElement!==fI)setVal('int'+i,t.intervalHours);});}
if(s.pump){$('pumpFaultWarn').style.display=s.pump.fault?'block':'none';}}
function updateVent(idx,v){if(!v)return;
var bar=$(idx===0?'vuBar':'vlBar'),pct=$(idx===0?'vuPct':'vlPct');
var p=Math.max(0,Math.min(100,v.current||0));
if(bar){bar.style.width=p+'%';
if(p>0.5){bar.style.backgroundSize=(10000/p)+'% 100%';}
else{bar.style.backgroundSize='100% 100%';}}
if(pct)pct.textContent=p+' %';}

// v6.1: перекраска кнопок по положению створок удалена.
// Она красила ОБЕ кнопки одним цветом (обе красные на закрытых форточках,
// обе зелёные на открытых), поэтому цвет не сообщал, что делает кнопка,
// и спорил с языком панели, где зелёный = включено, красный = выключено.
// Теперь цвет закреплён за действием в CSS: .vent-btn-open / .vent-btn-close.

// Форматирует countdown как DD HH:MM (сутки часы:минуты)
function fmtCdParts(totalSec){totalSec=Math.max(0,totalSec|0);var totalMin=Math.ceil(totalSec/60);var d=Math.floor(totalMin/(60*24));var h=Math.floor((totalMin%(60*24))/60);var m=totalMin%60;return {d:pad2(d),h:pad2(h),m:pad2(m)};}function onTimerState(n,active,remaining,nextStart){var el=$('cd'+n);if(!el)return;if(active){el.innerHTML='<span class="cd-active">'+i18n('timer_active')+'</span>';el.className='countdown active';}else if(nextStart>=0){var f=fmtCdParts(nextStart);el.innerHTML='<span>'+f.d+'</span> <span>'+f.h+'</span><span class="seg-colon">:</span><span>'+f.m+'</span>';el.className='countdown';}else{el.innerHTML='<span>--</span> <span>--</span><span class="seg-colon">:</span><span>--</span>';el.className='countdown';}}// v6.1: подсвечиваем ту подпись тумблера, которая соответствует
// текущему режиму. Левая подпись — выключенное состояние чекбокса,
// правая — включённое.
// v6.1: у MQTT-тумблера подпись всего одна: справа от него не вторая
// половина, а состояние связи с брокером — это разные факты, и
// markToggle сюда не подходит.
// Слово справа от тумблера отвечает сразу на два вопроса: включён ли
// MQTT вообще и есть ли связь с брокером. Раньше оно показывало только
// связь, из-за чего читалось как вторая половина тумблера и врало.
// Цветовой сдвиг подписи в бежевой и коричневой темах почти незаметен
// (rgb(107,74,0) против rgb(92,64,0)) — состояние обязано быть словом.
var _mqttConn=false;
function renderMqttState(connected){var cb=$('mqttEnable'),d=$('mqttStatusDot');
if(!cb||!d)return;
if(connected!==undefined)_mqttConn=!!connected;
var k=!cb.checked?'set_mqtt_disabled':(_mqttConn?'set_mqtt_on':'set_mqtt_off');
d.setAttribute('data-i18n',k);d.textContent=i18n(k);
d.style.color=!cb.checked?'var(--text-muted)':(_mqttConn?'var(--on-col)':'var(--warn)');}
function markMqttEnable(){var cb=$('mqttEnable');if(!cb)return;
var w=cb.closest('.dhcp-row');if(!w)return;
var l=w.querySelector('.dhcp-lbl');if(l)l.classList.toggle('on',cb.checked);}
function markToggle(cbId,rightActive,sel){
  var cb=$(cbId); if(!cb)return;
  var wrap=cb.closest('.mode-wrap')||cb.closest('.dhcp-row'); if(!wrap)return;
  var l=wrap.querySelectorAll(sel||'.mode-lbl');
  if(l.length<2)return;
  l[0].classList.toggle('on',!rightActive);
  l[1].classList.toggle('on',!!rightActive);
}
function applyModeVents(a){markToggle('modeVents',a);var cb=$('modeVents');if(cb)cb.checked=a;var av=$('ventsAuto'),vc=$('ventsCalib'),vsr=$('ventAutoStateRow'),vmb=$('ventManualBtns');if(av)av.style.display=a?'block':'none';if(vc)vc.style.display=a?'block':'none';if(vsr)vsr.style.display=a?'block':'none';if(vmb)vmb.style.display=a?'none':'flex';}
function applyModeTimers(a){markToggle('modeTimers',a);var cb=$('modeTimers');if(cb)cb.checked=a;document.querySelectorAll('#pageDashboard .auto-mode').forEach(function(e){e.style.display=a?'block':'none';});document.querySelectorAll('#pageDashboard .manual-mode').forEach(function(e){e.style.display=a?'none':'block';});}
function applyModeHumid(a){markToggle('modeHumid',a);var cb=$('modeHumid');if(cb)cb.checked=a;var av=$('humidAuto'),mv=$('humidManual');if(av)av.style.display=a?'block':'none';if(mv)mv.style.display=a?'none':'block';}
function applyModePump(a){markToggle('modePump',a);var cb=$('modePump');if(cb)cb.checked=a;var av=$('pumpAuto'),mv=$('pumpManual');if(av)av.style.display=a?'block':'none';if(mv)mv.style.display=a?'none':'block';}
var LUX_NIGHT=30,LUX_DAY=4000;function updateLuxGif(lux,luxOk,rain){var moon=$('moonGif'),cloud=$('cloudGif'),sun=$('sunGif'),rg=$('rainGif');[moon,cloud,sun,rg].forEach(function(x){if(x)x.classList.remove('visible');});if(rain){var n=luxOk&&lux<LUX_NIGHT;if(moon&&n)moon.classList.add('visible');if(rg)rg.classList.add('visible');return;}
if(!luxOk){if(sun)sun.classList.add('visible');return;}
if(lux<LUX_NIGHT){moon&&moon.classList.add('visible');}
else if(lux<=LUX_DAY){cloud&&cloud.classList.add('visible');}
else{sun&&sun.classList.add('visible');}}
function toggleRelay(n){var now=Date.now();if(_btnLastMs[n]&&now-_btnLastMs[n]<500)return;_btnLastMs[n]=now;var btn=$('rBtn'+n);if(!btn)return;var ns=btn.classList.contains('off');wsSend({cmd:'set_relay',idx:n,state:ns});}
window.toggleRelay=toggleRelay;// v6.0: автокалибровка убрана. Калибровка теперь — это просто
// сохранение нового значения времени хода (travelMs) в NVS.
function applyVentAuto(){var op=parseFloat($('vTOpen').value),cl=parseFloat($('vTClose').value);var tu=parseInt($('vuTravel').value,10),tl=parseInt($('vlTravel').value,10);if(isNaN(op)||isNaN(cl)){toast(i18n('err_fill_all'),'warning');return;}if(cl>=op){toast(i18n('err_close_lt_open'),'warning');return;}if(isNaN(tu)||tu<1||tu>120||isNaN(tl)||tl<1||tl>120){toast(i18n('err_calib_range'),'warning');return;}var nSent=0;if(op!==_lastFormVent.vTOpen||cl!==_lastFormVent.vTClose){wsSend({cmd:'set_thresholds',ventUpperHigh:op,ventUpperLow:cl,ventLowerHigh:op,ventLowerLow:cl});_lastFormVent.vTOpen=op;_lastFormVent.vTClose=cl;nSent++;}if(tu!==_lastFormVent.vuTravel){wsSend({cmd:'vent_set_calib',side:'top',sec:tu});_lastFormVent.vuTravel=tu;nSent++;}if(tl!==_lastFormVent.vlTravel){wsSend({cmd:'vent_set_calib',side:'bottom',sec:tl});_lastFormVent.vlTravel=tl;nSent++;}if(nSent>0)toast(i18n('toast_saved'),'success');else toast(i18n('msg_no_changes'),'info');}window.applyVentAuto=applyVentAuto;
// v6.1: калибровочный прогон створки.
// Позиция считается по времени работы привода и со временем расходится
// с реальностью. Прогон гонит створку на закрытие полное время хода
// с запасом, до упора в концевик, и обнуляет счётчик. Раньше это
// лечилось только полным сбросом настроек.
function ventHome(side){
  if(!confirm(i18n('toast_confirm_home')))return;
  if(wsSend({cmd:'vent_home',side:side}))toast(i18n('toast_home_started'),'info');
}
window.ventHome=ventHome;
function ventHomeCancel(){wsSend({cmd:'vent_home_cancel'});}
window.ventHomeCancel=ventHomeCancel;
function applyVentCalib(side){var fieldId=side==='top'?'vuTravel':'vlTravel';var sec=parseInt($(fieldId).value,10);if(isNaN(sec)||sec<1||sec>120){toast(i18n('err_calib_range'),'warning');return;}wsSend({cmd:'vent_set_calib',side:side,sec:sec});toast(i18n('toast_saved'),'success');}window.applyVentCalib=applyVentCalib;function pumpResetFault(){if(wsSend({cmd:'force_pump_reset'}))toast(i18n('toast_fault_reset'),'success');}
window.pumpResetFault=pumpResetFault;function applyVentThresholds(){var op=parseFloat($('vTOpen').value),cl=parseFloat($('vTClose').value);if(isNaN(op)||isNaN(cl)){toast(i18n('err_fill_all'),'warning');return;}if(cl>=op){toast(i18n('err_close_lt_open'),'warning');return;}wsSend({cmd:'set_thresholds',ventUpperHigh:op,ventUpperLow:cl,ventLowerHigh:op,ventLowerLow:cl});toast(i18n('toast_saved'),'success');}
window.applyVentThresholds=applyVentThresholds;function applyHumidThresholds(){var hmH=parseFloat($('hmH').value),hmL=parseFloat($('hmL').value);if(isNaN(hmH)||isNaN(hmL)||hmL>=hmH){toast(i18n('err_bad_thresholds'),'warning');return;}
wsSend({cmd:'set_thresholds',humidHigh:hmH,humidLow:hmL});toast(i18n('toast_saved'),'success');}
window.applyHumidThresholds=applyHumidThresholds;function applyWaterCfg(){var lp=parseInt($('wtLP').value,10),hp=parseInt($('wtHP').value,10);var dp=parseInt($('wtDp').value,10),tm=parseInt($('wtTm').value,10);if([lp,hp,dp,tm].some(isNaN)){toast(i18n('err_fill_all'),'warning');return;}
if(lp>=hp){toast(i18n('err_on_lt_off'),'warning');return;}
wsSend({cmd:'set_water_tank',lowPct:lp,highPct:hp,depthCm:dp,pumpTimeoutMin:tm});toast(i18n('toast_saved'),'success');}
window.applyWaterCfg=applyWaterCfg;function applyAllTimers(){for(var n=0;n<3;n++){var sv=($('start'+n).value||'').split(':').map(Number);var sm=(sv[0]||0)*60+(sv[1]||0);var dur=parseInt($('dur'+n).value,10);var iv=parseInt($('int'+n).value,10);if(isNaN(dur)||isNaN(iv))continue;wsSend({cmd:'set_timer',idx:n,startMin:sm,durationMin:dur,intervalHours:iv});}
toast(i18n('toast_saved'),'success');}
window.applyAllTimers=applyAllTimers;function onModeChange(key,checked){wsSend({cmd:'set_mode',key:key,value:checked?1:0});}

// v6.0: hold-кнопки форточек (pointer events)
// v6.1: те же кнопки работают с клавиатуры — Space и Enter удерживают
// привод, отпускание клавиши останавливает. preventDefault на keydown
// гасит и прокрутку страницы пробелом, и синтетический click, который
// браузер иначе шлёт кнопке. Флаг held защищает от автоповтора клавиши
// и от парных stop (pointerup + pointerleave приходят вместе).
// Потеря фокуса и уход вкладки в фон тоже останавливают: иначе привод
// остался бы в движении до таймаута MANUAL_HOLD_TIMEOUT_MS в прошивке.
function bindVentHold(btn,type){
if(!btn)return null;
var held=false;
function start(){if(held)return;held=true;wsSend({cmd:'vent_hold',cmd_type:type});}
function stop(){if(!held)return;held=false;wsSend({cmd:'vent_hold',cmd_type:'stop'});}
btn.addEventListener('pointerdown',function(e){e.preventDefault();start();btn.setPointerCapture&&btn.setPointerCapture(e.pointerId);});
['pointerup','pointercancel','pointerleave'].forEach(function(ev){btn.addEventListener(ev,stop);});
function isAct(e){return e.key===' '||e.key==='Spacebar'||e.key==='Enter';}
btn.addEventListener('keydown',function(e){if(!isAct(e))return;e.preventDefault();start();});
btn.addEventListener('keyup',function(e){if(!isAct(e))return;e.preventDefault();stop();});
btn.addEventListener('blur',stop);
return stop;
}
function setupVentHoldButtons(){
var stopO=bindVentHold($('vBtnOpen'),'open');
var stopC=bindVentHold($('vBtnClose'),'close');
function stopAll(){if(stopO)stopO();if(stopC)stopC();}
document.addEventListener('visibilitychange',function(){if(document.hidden)stopAll();});
window.addEventListener('blur',stopAll);
}

// v6.1: удалена setupVentSliders — мёртвый код от слайдеров позиции,
// убранных в v6.0. Она обращалась к ventSliderLocked, которая нигде
// не объявлена: любой вызов бросил бы ReferenceError.
const GAUGES=[{id:'cTemp',valId:'gv0',min:-10,max:40,value:NaN,unit:'\u00b0C',fmt:function(v){return v.toFixed(1);},zones:[{to:18,c:'#38bdf8'},{to:28,c:'#4ade80'},{to:40,c:'#f87171'}]},{id:'cHum',valId:'gv1',min:10,max:100,value:NaN,unit:'%',fmt:function(v){return v.toFixed(1);},zones:[{to:50,c:'#38bdf8'},{to:79,c:'#4ade80'},{to:100,c:'#f87171'}]},{id:'cPress',valId:'gv2',min:743,max:770,value:NaN,unit:'mm',fmt:function(v){return Math.round(v)+'';},zones:[{to:748,c:'#38bdf8'},{to:760,c:'#4ade80'},{to:770,c:'#f87171'}]}];function zoneColor(v,z){for(var i=0;i<z.length;i++){if(v<=z[i].to)return z[i].c;}return z[z.length-1].c;}
function updateGaugeDom(g,value){var el=g.valId?$(g.valId):null;if(!el)return;var unitLbl=g.id==='cPress'?(currentLang==='ru'?'мм':'mm'):(g.id==='cTemp'?'\u00b0C':'%');el.textContent=(isNaN(value)||value===null)?('-- '+unitLbl):(g.fmt(value)+' '+unitLbl);el.style.color=(isNaN(value)||value===null)?'var(--text-muted)':zoneColor(value,g.zones);}
function drawGauge(g){var cv=$(g.id);if(!cv)return;var ctx=cv.getContext('2d'),dpr=window.devicePixelRatio||1;var W=cv.getBoundingClientRect().width||175,H=cv.getBoundingClientRect().height||88;if(cv.width!==Math.round(W*dpr)||cv.height!==Math.round(H*dpr)){cv.width=Math.round(W*dpr);cv.height=Math.round(H*dpr);}
ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,W,H);var cx=W/2,cy=H-4,sw=22,R=Math.min(cx-sw/2-2,cy-sw/2-2);var frac=isNaN(g.value)?0:Math.max(0,Math.min(1,(g.value-g.min)/(g.max-g.min)));ctx.save();ctx.lineCap='round';ctx.lineWidth=sw;ctx.strokeStyle='#c8d8c8';ctx.beginPath();ctx.arc(cx,cy,R,Math.PI,0);ctx.stroke();ctx.restore();if(!isNaN(g.value)&&frac>0.005){var col=zoneColor(g.value,g.zones);ctx.save();ctx.lineWidth=sw-6;ctx.strokeStyle=col;ctx.lineCap='butt';ctx.beginPath();ctx.arc(cx,cy,R,Math.PI,Math.PI+Math.PI*frac);ctx.stroke();ctx.beginPath();ctx.arc(cx-R,cy,sw/2-3,0,Math.PI*2);ctx.fillStyle=col;ctx.fill();ctx.restore();}
if(!isNaN(g.value)){var ea=Math.PI-Math.PI*frac,Ro=R+sw/2-1;var ax=cx+Ro*Math.cos(ea),ay=cy-Ro*Math.sin(ea);var dx=cx-ax,dy=cy-ay;var len=Math.sqrt(dx*dx+dy*dy)*0.96;var d2=Math.sqrt(dx*dx+dy*dy)||1;var bx=ax+dx/d2*len,by=ay+dy/d2*len;var px=-dy/d2,py=dx/d2;ctx.save();ctx.lineJoin='round';ctx.beginPath();ctx.moveTo(ax+px*0.9,ay+py*0.9);ctx.lineTo(bx+px*1.6,by+py*1.6);ctx.lineTo(bx-px*1.6,by-py*1.6);ctx.lineTo(ax-px*0.9,ay-py*0.9);ctx.closePath();ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.stroke();ctx.beginPath();ctx.moveTo(ax+px*0.9,ay+py*0.9);ctx.lineTo(bx+px*1.6,by+py*1.6);ctx.lineTo(bx-px*1.6,by-py*1.6);ctx.lineTo(ax-px*0.9,ay-py*0.9);ctx.closePath();ctx.fillStyle='#1a2e1a';ctx.fill();ctx.restore();}}
function animateTo(g,target,dur){var from=isNaN(g.value)?g.min:g.value,t0=performance.now();(function step(now){var tk=Math.min(1,(now-t0)/dur);var e=tk<0.5?2*tk*tk:-1+(4-2*tk)*tk;g.value=from+(target-from)*e;drawGauge(g);if(tk<1)requestAnimationFrame(step);})(t0);}
function redrawAll(){GAUGES.forEach(function(g){drawGauge(g);});}
function renderHistory(points){if(!Array.isArray(points)||!points.length){histData=[];drawHistChart(activeChart);return;}
histData=points.map(function(p){var d=new Date(p.t*1000);return{ts:p.t,h:d.getHours(),t:p.T,hu:p.H,p:p.P,w:p.W,l:p.L,t2:p.T2,hu2:p.H2};});drawHistChart(activeChart);}
function showChart(type,btn){activeChart=type;document.querySelectorAll('.chart-tab').forEach(function(b){b.classList.remove('active');});if(btn)btn.classList.add('active');drawHistChart(type);}
window.showChart=showChart;function drawHistChart(type){
var cv=$('histChart'),empty=$('chartEmpty');if(!cv)return;
if(!histData||histData.length<2){if(empty)empty.style.display='block';cv.style.display='none';return;}
if(empty)empty.style.display='none';cv.style.display='block';
var ctx=cv.getContext('2d'),dpr=window.devicePixelRatio||1;
var W=cv.parentElement.offsetWidth||600,H=cv.parentElement.offsetHeight||200;
cv.width=Math.round(W*dpr);cv.height=Math.round(H*dpr);
ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,W,H);
var series=[],UNIT='';
if(type==='temp'){series=[{key:'t',color:'#f87171',label:i18n('chart_inside')},{key:'t2',color:'#64748b',label:i18n('chart_outside'),dash:[5,4]}];UNIT='°C';}
else if(type==='hum'){series=[{key:'hu',color:'#38bdf8',label:i18n('chart_inside')},{key:'hu2',color:'#64748b',label:i18n('chart_outside'),dash:[5,4]}];UNIT='%';}
else if(type==='press'){series=[{key:'p',color:'#a78bfa',label:i18n('tab_press')}];UNIT=(currentLang==='ru'?'мм':'mm');}
else if(type==='water'){series=[{key:'w',color:'#4caf50',label:i18n('tab_water')}];UNIT='%';}
var st=getComputedStyle(document.documentElement);
var gridCol=st.getPropertyValue('--card-border').trim()||'#d4e0d4';
var textCol=st.getPropertyValue('--text-muted').trim()||'#7a9a7a';
var allVals=[],labels=[];
histData.forEach(function(p){labels.push(pad2(p.h)+':00');series.forEach(function(s){if(p[s.key]!=null&&!isNaN(p[s.key]))allVals.push(p[s.key]);});});
if(allVals.length<2){if(empty)empty.style.display='block';cv.style.display='none';return;}
var vmin=Math.min.apply(null,allVals),vmax=Math.max.apply(null,allVals),range=(vmax-vmin)||1;
var pad_v=range*0.05;vmin-=pad_v;vmax+=pad_v;range=vmax-vmin;
var pad={l:48,r:14,t:32,b:26},cw=W-pad.l-pad.r,ch=H-pad.t-pad.b,n=histData.length;
var xOf=function(i){return pad.l+(i/(n-1))*cw;};
var yOf=function(v){return pad.t+ch-((v-vmin)/range)*ch;};
ctx.save();ctx.strokeStyle=gridCol;ctx.lineWidth=1;ctx.setLineDash([4,4]);
for(var gi=0;gi<=4;gi++){var y=pad.t+(gi/4)*ch;ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(W-pad.r,y);ctx.stroke();
ctx.fillStyle=textCol;ctx.font='11px Arial';ctx.textAlign='right';ctx.textBaseline='middle';
ctx.fillText((vmax-(gi/4)*range).toFixed(1)+' '+UNIT,pad.l-4,y);}
ctx.restore();
series.forEach(function(s){
var pts=[];histData.forEach(function(p,i){if(p[s.key]!=null&&!isNaN(p[s.key]))pts.push({x:xOf(i),y:yOf(p[s.key])});});
if(pts.length<2)return;
if(s===series[0]&&series.length===1){
ctx.save();ctx.beginPath();ctx.moveTo(pts[0].x,pts[0].y);
for(var i=1;i<pts.length;i++)ctx.lineTo(pts[i].x,pts[i].y);
ctx.lineTo(pts[pts.length-1].x,H-pad.b);ctx.lineTo(pts[0].x,H-pad.b);ctx.closePath();
var grad=ctx.createLinearGradient(0,pad.t,0,H-pad.b);
grad.addColorStop(0,s.color+'55');grad.addColorStop(1,s.color+'00');
ctx.fillStyle=grad;ctx.fill();ctx.restore();}
ctx.save();ctx.beginPath();ctx.strokeStyle=s.color;ctx.lineWidth=2.2;ctx.lineJoin='round';
if(s.dash)ctx.setLineDash(s.dash);
ctx.moveTo(pts[0].x,pts[0].y);
for(var j=1;j<pts.length;j++)ctx.lineTo(pts[j].x,pts[j].y);
ctx.stroke();ctx.restore();
ctx.save();ctx.fillStyle='#fff';ctx.strokeStyle=s.color;ctx.lineWidth=2;
pts.forEach(function(pt){ctx.beginPath();ctx.arc(pt.x,pt.y,3.2,0,Math.PI*2);ctx.fill();ctx.stroke();});
ctx.restore();});
if(series.length>1){
var lx=pad.l,ly=10;ctx.save();ctx.font='11px Arial';ctx.textBaseline='middle';
series.forEach(function(s){
ctx.strokeStyle=s.color;ctx.fillStyle=s.color;ctx.lineWidth=2;
if(s.dash)ctx.setLineDash(s.dash);else ctx.setLineDash([]);
ctx.beginPath();ctx.moveTo(lx,ly);ctx.lineTo(lx+22,ly);ctx.stroke();
ctx.setLineDash([]);ctx.fillStyle=textCol;ctx.textAlign='left';
ctx.fillText(s.label,lx+28,ly);
lx+=ctx.measureText(s.label).width+50;});
ctx.restore();}
ctx.save();ctx.fillStyle=textCol;ctx.font='10px Arial';ctx.textAlign='center';
var step=Math.max(1,Math.floor(n/8));
for(var li=0;li<n;li+=step)ctx.fillText(labels[li],xOf(li),H-pad.b+14);
ctx.restore();
}
// v6.1: контроллер отвечает на 401 без заголовка WWW-Authenticate, поэтому
// системное окно браузера больше не всплывает. Обрабатываем отказ сами:
// сбрасываем сессию и возвращаем пользователя на форму входа панели.
function onAuthLost(){
  _authUser='';_authPass='';
  try{_ss.removeItem('gh_u');_ss.removeItem('gh_p');}catch(e){}
  stopStatePoll();
  if(ws&&ws.readyState===WebSocket.OPEN)ws.close();
  showLoginScreen();
  var err=$('loginError');
  if(err){err.textContent=i18n('ws_auth_fail');err.style.display='block';}
}
async function fetchJson(url,options){var opts=options||{};if(!opts.headers||!opts.headers['Authorization']){opts.headers=authHeaders(opts.headers||{});}
var resp=await fetch(url,opts);
if(resp.status===401){onAuthLost();throw new Error('HTTP 401');}
if(!resp.ok)throw new Error('HTTP '+resp.status);return await resp.json();}
async function loadAllSettings(){try{var s=await fetchJson('/api/settings');fillSettingsForm(s);}catch(e){toast(i18n('err_load_settings')+': '+e.message,'error');}
await loadNetSettings();await loadMqttSettings();}
function safeSet(id,v){var el=$(id);if(!el||v===undefined||v===null)return;if(document.activeElement===el)return;el.value=v;}function fillSettingsForm(s){if(s.ventUpper){safeSet('vTOpen',s.ventUpper.high);safeSet('vTClose',s.ventUpper.low);_lastFormVent.vTOpen=s.ventUpper.high;_lastFormVent.vTClose=s.ventUpper.low;if(s.ventUpper.travelMs!==undefined){var tuv=Math.round(s.ventUpper.travelMs/1000);safeSet('vuTravel',tuv);_lastFormVent.vuTravel=tuv;}}if(s.ventLower){if(s.ventLower.travelMs!==undefined){var tlv=Math.round(s.ventLower.travelMs/1000);safeSet('vlTravel',tlv);_lastFormVent.vlTravel=tlv;}}if(s.humid){safeSet('hmL',s.humid.low);safeSet('hmH',s.humid.high);}if(s.waterTank){safeSet('wtLP',s.waterTank.lowPct);safeSet('wtHP',s.waterTank.highPct);safeSet('wtDp',s.waterTank.depthCm);safeSet('wtTm',s.waterTank.pumpTimeoutMin);}
if(s.timers){s.timers.forEach(function(t,i){var h=Math.floor(t.startMin/60),m=t.startMin%60;setVal('start'+i,pad2(h)+':'+pad2(m));setVal('dur'+i,t.durationMin);setVal('int'+i,t.intervalHours);});}
if(s.ntp){setVal('ntpServer',s.ntp.server);if(s.ntp.tz)selectTzByPosix(s.ntp.tz);if(s.ntp.updateHrs)setVal('ntpUpdateSel',s.ntp.updateHrs);}
// v6.1: язык из контроллера надо ПРИМЕНИТЬ, а не только выставить
// в выпадающем списке. Раньше здесь стоял setVal, и после смены IP
// панель оставалась английской, показывая в настройках «Русский»:
// localStorage привязан к origin, новый адрес — новое хранилище,
// сохранённый выбор туда не переезжает. Тема этим не страдала, у неё
// вызывался applyTheme. Записываем обратно в localStorage, чтобы
// на этом адресе язык применялся сразу, не дожидаясь /api/settings.
if(s.ui){
if(s.ui.theme!==undefined){applyTheme(s.ui.theme);
try{localStorage.setItem('gh_theme',s.ui.theme||'');}catch(e){}}
if(s.ui.lang&&GH_I18N[s.ui.lang]){
if(s.ui.lang!==currentLang)applyLang(s.ui.lang);else setVal('langSel',s.ui.lang);
try{localStorage.setItem(LANG_KEY,s.ui.lang);}catch(e){}}}
if(s.fwVersion)setFwVersion(s.fwVersion);}
// v6.1: номер версии приходит от контроллера (FW_VERSION_MAJOR/MINOR из
// config.h) и раскладывается по всем трём местам сразу. Раньше в модалке
// «О программе» он был вписан в словарь переводов руками и отстал на два
// релиза, показывая 6.0 на прошивке 6.1.
function setFwVersion(v){
  if(!v)return;
  ['fwVersion','infoFw','aboutVersion'].forEach(function(id){
    var el=$(id); if(el)el.textContent=v;
  });
}
var TZ_POSIX_MAP={'UTC+12':'UTC-12','UTC+11':'UTC-11','UTC+10':'UTC-10','UTC+9':'UTC-9','UTC+8':'UTC-8','UTC+7':'UTC-7','UTC+6':'UTC-6','UTC+5':'UTC-5','UTC+4':'UTC-4','UTC+3':'UTC-3','UTC+2':'UTC-2','UTC+1':'UTC-1','UTC0':'UTC+0','UTC-1':'UTC+1','UTC-2':'UTC+2','MSK-3':'MSK','UTC-4':'UTC+4','UTC-5':'UTC+5','UTC-6':'UTC+6','UTC-7':'UTC+7','UTC-8':'UTC+8','UTC-9':'UTC+9','UTC-10':'UTC+10','UTC-11':'UTC+11','UTC-12':'UTC+12'};function selectTzByPosix(posix){var sel=$('ntpTzSel');if(!sel)return;var val=TZ_POSIX_MAP[posix];if(val)setVal('ntpTzSel',val);}
function onTzSelChange(){var sel=$('ntpTzSel');if(!sel)return;var opt=sel.options[sel.selectedIndex];if(opt){var ntpStatus=$('ntpStatus');if(ntpStatus)ntpStatus.textContent='POSIX: '+opt.getAttribute('data-posix');}}
window.onTzSelChange=onTzSelChange;async function applyNtp(){var server=($('ntpServer').value||'pool.ntp.org').trim();var sel=$('ntpTzSel');if(!sel){toast(i18n('err_generic'),'error');return;}
var opt=sel.options[sel.selectedIndex];var posix=opt?opt.getAttribute('data-posix'):'UTC0';var hrs=parseInt(($('ntpUpdateSel').value||'4'),10);try{var r=await fetchJson('/api/ntp',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({server:server,tz:posix,updateHrs:hrs})});if(r.ok)toast(i18n('toast_saved'),'success');else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyNtp=applyNtp;async function loadNetSettings(){try{var n=await fetchJson('/api/network');setVal('ssid',n.ssid);setVal('wPas','');$('modeSwitch').checked=!n.isDHCP;onDhcpModeChange();setVal('ipAddr',n.ip);setVal('subnet',n.subnet);setVal('gateway',n.gateway);setVal('dns1',n.dns1);setVal('dns2',n.dns2);$('infoMac')&&($('infoMac').textContent=n.mac||'—');}catch(e){toast(i18n('err_load_network'),'error');}}
window.loadNetSettings=loadNetSettings;async function loadMqttSettings(){try{var m=await fetchJson('/api/mqtt');$('mqttEnable').checked=!!m.enabled;markMqttEnable();setVal('mqttServer',m.server);setVal('mqttPort',m.port);setVal('mqttUser',m.user);setVal('mqttPass','');renderMqttState(m.connected);}catch(e){toast(i18n('err_load_mqtt'),'error');}}
window.loadMqttSettings=loadMqttSettings;function onDhcpModeChange(){var isStatic=$('modeSwitch').checked;markToggle('modeSwitch',isStatic,'.dhcp-lbl');$('staticFields').style.display=isStatic?'block':'none';$('dhcpHint').style.display=isStatic?'none':'block';}
window.onDhcpModeChange=onDhcpModeChange;async function applyNet(){var body={ssid:$('ssid').value,pass:$('wPas').value,isDHCP:!$('modeSwitch').checked,ip:$('ipAddr').value,subnet:$('subnet').value,gateway:$('gateway').value,dns1:$('dns1').value,dns2:$('dns2').value};try{var r=await fetchJson('/api/network',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(r.ok)toast(i18n('toast_saved')+', rebooting...','success');else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyNet=applyNet;async function resetWifi(){if(!confirm(i18n('toast_confirm_forget_wifi')))return;try{await fetchJson('/api/reset_wifi',{method:'POST'});toast(i18n('msg_rebooting'),'info');}
catch(e){toast(e.message,'error');}}
window.resetWifi=resetWifi;function scanWifi(){var list=$('wifiScanList');if(!list)return;list.style.display='block';list.textContent=i18n('wifi_scanning');fetchJson('/api/wifi_scan').then(function(d){if(!d.networks||!d.networks.length){list.textContent=i18n('wifi_none');return;}
list.innerHTML=d.networks.map(function(n){return'<div class="wifi-item" onclick="setVal(\'ssid\',\''+n.ssid+'\')">'+
n.ssid+' ('+n.rssi+' dBm'+(n.enc?' 🔒':'')+')'+'</div>';}).join('');}).catch(function(){list.textContent=i18n('wifi_scan_failed');});}
window.scanWifi=scanWifi;async function applyMqtt(){var body={enabled:$('mqttEnable').checked,server:$('mqttServer').value,port:parseInt($('mqttPort').value,10),user:$('mqttUser').value,pass:$('mqttPass').value};try{var r=await fetchJson('/api/mqtt',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(r.ok)toast(i18n('toast_saved'),'success');else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyMqtt=applyMqtt;function mqttResendDiscovery(){if(wsSend({cmd:'mqtt_resend_discovery'}))toast(i18n('toast_discovery_sent'),'success');}
window.mqttResendDiscovery=mqttResendDiscovery;async function applyWebCreds(){var u=$('wbUser').value.trim(),p=$('wbPass').value;if(u.length<1||p.length<4){toast(i18n('err_creds_len'),'warning');return;}
try{var r=await fetchJson('/api/webcreds',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({user:u,pass:p})});if(r.ok){toast(i18n('toast_saved'),'success');_authUser=u;_authPass=p;_ss.setItem('gh_u',u);_ss.setItem('gh_p',p);}else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyWebCreds=applyWebCreds;async function sysReboot(){if(!confirm(i18n('toast_confirm_reboot')))return;try{await fetchJson('/api/reboot',{method:'POST'});toast(i18n('msg_rebooting'),'info');}
catch(e){toast(e.message,'error');}}
window.sysReboot=sysReboot;
// v6.1: было fetchJson('/api/clear_history') — такого маршрута в прошивке
// НЕТ, кнопка возвращала 404 и историю не чистила. Команда clear_history
// в WebSocket при этом существовала и работала. Переведено на неё.
function sysClearHistory(){if(!confirm(i18n('toast_confirm_clear')))return;if(wsSend({cmd:'clear_history'})){histData=[];drawHistChart(activeChart);toast(i18n('toast_saved'),'success');}}
window.sysClearHistory=sysClearHistory;async function sysFactoryReset(){if(!confirm(i18n('toast_confirm_factory')))return;if(!confirm(i18n('toast_confirm_factory2')))return;try{await fetchJson('/api/factory_reset',{method:'POST'});toast(i18n('msg_resetting'),'warning');}
catch(e){toast(e.message,'error');}}
window.sysFactoryReset=sysFactoryReset;function otaValidateFile(){var fInput=$('otaFile'),f=fInput.files[0];var det=$('otaDetected'),btn=$('btnOtaUpload'),nameEl=$('otaFileName');if(!f){if(nameEl)nameEl.textContent=i18n('set_ota_no_file');det.textContent='';btn.disabled=true;return null;}if(nameEl)nameEl.textContent=f.name;var rx=/^greenhouse_v(\d+)\.(\d+)\.bin$/i;var m=rx.exec(f.name);if(m){det.textContent=i18n('ota_detected_container').replace('{v}','v'+m[1]+'.'+m[2]);det.style.color='var(--on-col)';btn.disabled=false;return 'container';}else{det.textContent=i18n('ota_bad_name');det.style.color='var(--off-col)';btn.disabled=true;return null;}}
window.otaValidateFile=otaValidateFile;function otaUpload(){var type=otaValidateFile();if(!type){toast(i18n('ota_bad_name'),'error');return;}var file=$('otaFile').files[0];var fd=new FormData();fd.append('firmware',file,file.name);$('otaProgress').style.display='block';$('btnOtaUpload').disabled=true;var xhr=new XMLHttpRequest();xhr.open('POST','/update');xhr.setRequestHeader('Authorization','Basic '+btoa(_authUser+':'+_authPass));xhr.upload.addEventListener('progress',function(e){if(e.lengthComputable){var pct=Math.round(e.loaded/e.total*100);$('otaBar').style.width=pct+'%';$('otaText').textContent=pct+'%';}});xhr.addEventListener('load',function(){$('btnOtaUpload').disabled=false;if(xhr.status===200){toast(i18n('ota_done'),'success');setTimeout(function(){location.reload();},10000);}else{var err=i18n('ota_failed')+' ('+xhr.status+')';try{var r=JSON.parse(xhr.responseText);if(r.error)err=r.error;}catch(e){}toast(err,'error');}});xhr.addEventListener('error',function(){$('btnOtaUpload').disabled=false;toast(i18n('ota_failed'),'error');});xhr.send(fd);}
window.otaUpload=otaUpload;function openAbout(){$('aboutModal').classList.add('open');$('aboutOverlay').classList.add('open');}
function closeAbout(){$('aboutModal').classList.remove('open');$('aboutOverlay').classList.remove('open');}
window.openAbout=openAbout;window.closeAbout=closeAbout;document.addEventListener('DOMContentLoaded',function(){try{var l=localStorage.getItem(LANG_KEY);if(l&&GH_I18N[l])currentLang=l;}catch(e){}
try{var t=localStorage.getItem('gh_theme');if(t!=null)applyTheme(t);}catch(e){}
applyLang(currentLang);[$('loginUser'),$('loginPass')].forEach(function(el){if(el)el.addEventListener('keydown',loginKeydown);});if(_authUser&&_authPass){showAppShell();wsConnect();loadAllSettings();}else{showLoginScreen();}['vents','timers','humid','pump'].forEach(function(key){var el=$('mode'+key.charAt(0).toUpperCase()+key.slice(1));if(el)el.addEventListener('change',function(){onModeChange(key,el.checked);});});setupVentHoldButtons();setupTabsHint();setupThemeTiles();
(function(){var mq=$('mqttEnable');if(mq)mq.addEventListener('change',function(){markMqttEnable();renderMqttState();});})();var al=$('aboutLink');if(al)al.addEventListener('click',function(e){e.preventDefault();openAbout();});document.addEventListener('keydown',function(e){if(e.key==='Escape'){closeAbout();closeSidebar();}});window.addEventListener('beforeunload',function(){clearTimeout(wsTimer);stopStatePoll();if(ws)ws.close();});setTimeout(redrawAll,100);window.addEventListener('resize',redrawAll);});