'use strict';const LANG_KEY='gh_lang';const GH_I18N={en:{nav_control:'Dashboard',nav_settings:'Settings',nav_about:'About',nav_history:'History',nav_events:'Event log',ev_hint:'Up to 32 important and 48 routine events; alarms are not pushed out by routine ones. Refreshed every 10 seconds while this page is open. The period only filters the view — nothing is deleted.',ev_all:'All',ev_p1:'24 h',ev_p7:'7 days',ev_p30:'30 days',ev_alarms:'Alarms',ev_water:'Water',ev_vents:'Vents',ev_sys:'System',ev_empty:'No events yet',ev_today:'Today',ev_yesterday:'Yesterday',ev_notime:'Before clock sync',ev_boot:'Controller started — {r}',ev_on:'{n}: on',ev_off:'{n}: off',ev_vent:'{n}: {a} → {b} %',ev_upper:'Upper vent',ev_lower:'Lower vent',ev_mode_auto:'{n}: Auto mode',ev_mode_manual:'{n}: Manual mode',ev_pump_fault:'Pump fault',ev_pump_ok:'Pump fault cleared',ev_sens_fail:'Sensor {n}: fault',ev_sens_ok:'Sensor {n}: working again',ev_rain_on:'Rain started',ev_rain_off:'Rain stopped',ev_wifi_off:'Wi-Fi: connection lost',ev_wifi_on:'Wi-Fi: connected',ev_auth:'Failed sign-in attempts: {n}',ev_set:'Settings changed: {n}',ev_ota_ok:'Firmware update installed',ev_ota_fail:'Firmware update failed',ev_reboot_panel:'Reboot from the panel',ev_reboot_plan:'Scheduled reboot',ev_hist_cleared:'History cleared',evs_0:'thresholds',evs_1:'irrigation timers',evs_2:'water tank',evs_3:'network',evs_4:'MQTT',evs_5:'panel login and password',evs_6:'time (NTP)',evs_7:'vent calibration',evs_8:'factory reset',evn_irr1:'Irrigation 1',evn_irr2:'Irrigation 2',evn_irr3:'Irrigation 3',evn_humid:'Humidifier',evn_pump:'Tank pump',evm_0:'Vents',evm_1:'Irrigation',evm_2:'Humidifier',evm_3:'Tank pump',nav_system:'System status',hist_stats:'For the selected period',st_series:'Series',st_min:'Min',st_avg:'Average',st_max:'Max',st_last:'Latest',st_none:'No data for this period',sys_hint:'Refreshed every 5 seconds while this page is open.',sys_run:'Operation',sys_fw:'Firmware',sys_uptime:'Uptime',sys_reset:'Last start',sys_time:'Clock (NTP)',sys_synced:'synchronized',sys_not_synced:'not synchronized',sys_mem:'Memory',sys_heap:'Free',sys_heap_min:'Minimum since start',sys_largest:'Largest block',sys_nvs:'Settings memory (NVS)',sys_nvs_val:'{u} of {t} entries used',sys_nvs_wr:'NVS writes since start',sys_net:'Network',sys_ssid:'Wi-Fi network',sys_signal:'Signal',sys_ip:'IP address',sys_mac:'MAC',sys_mqtt:'MQTT',sys_on:'connected',sys_off:'not connected',sys_rescue:'Rescue access point',sys_rescue_on:'on',sys_rescue_off:'off',sys_sensors:'Sensors',sys_ok:'working',sys_fail:'fault',sys_stack:'Task stack headroom',sys_stack_core1:'Control loop',sys_stack_loop:'Main loop (MQTT, OTA)',sys_stack_tcp:'Web server',sys_stack_wifi:'Wi-Fi watchdog',sys_absent:'not running',rr_POWERON:'power on',rr_SW:'software restart (update, button, schedule)',rr_EXT:'reset button',rr_PANIC:'program crash',rr_INT_WDT:'watchdog (hang)',rr_TASK_WDT:'watchdog (hang)',rr_WDT:'watchdog (hang)',rr_BROWNOUT:'power dip',rr_DEEPSLEEP:'wake from sleep',rr_UNKNOWN:'unknown',up_d:'d',btn_logout:'Logout',lbl_time:'TIME',lbl_uptime:'UPTIME',lbl_sensors:'Sensors',lbl_temp:'Temperature',lbl_hum:'Humidity',lbl_press:'Pressure',lux_unit:'lx',lbl_vents:'Vents',lbl_upper_travel:'Upper (42 sec)',lbl_lower_travel:'Lower (24 sec)',lbl_upper:'Upper vent',lbl_lower:'Lower vent',lbl_closed:'Closed',lbl_opened:'Open',btn_calibrate:'Calibrate',lbl_travel_sec:'Travel time, sec',btn_vent_open:'OPEN',btn_vent_close:'CLOSE',lbl_temp_open:'Open at, °C',lbl_temp_close:'Close at, °C',lbl_calibration:'Calibration (full travel time)',lbl_calib_top:'Upper, sec',lbl_calib_bottom:'Lower, sec',lbl_rain:'Rain',vstate_idle:'Idle',vstate_open_top:'Opening upper...',vstate_open_bottom:'Opening lower...',vstate_close_top:'Closing upper...',vstate_close_bottom:'Closing lower...',vstate_pause:'Pause (T measure)...',vstate_rain:'Rain — emergency close',lbl_vent_open_t:'Open at, \u00b0C',lbl_vent_close_t:'Close at, \u00b0C',lbl_irrigation:'Irrigation',lbl_irr1:'Irrigation 1',lbl_irr2:'Irrigation 2',lbl_irr3:'Irrigation 3',lbl_start:'Start time',lbl_dur:'Duration, min',lbl_interval:'Interval, h',timer_active:'ACTIVE',lbl_apply:'Apply',lbl_apply_irr:'\u2699 Apply timer settings',lbl_apply_hum:'Apply',lbl_apply_water:'Apply',lbl_humidifier:'Humidifier',lbl_state:'State',cd_units:'until start: days h:min',lbl_humid_on:'On',lbl_humid_off:'Off',lbl_hum_off_pct:'Off at humidity, %',lbl_hum_on_pct:'On at humidity, %',lbl_water_tank:'Water tank',lbl_pump_state:'Pump',lbl_pump_on:'On',lbl_pump_off:'Off',lbl_pump_fault:'Pump fault \u2014 check sensor',btn_reset_fault:'Reset',lbl_pump_on_pct:'On at, %',lbl_pump_off_pct:'Off at, %',lbl_tank_depth:'Depth, cm',lbl_pump_timeout:'Timeout, min',lbl_history:'History',hist_day:'Day',hist_week:'Week',tab_temp:'Temperature',tab_hum:'Humidity',tab_press:'Pressure',tab_water:'Water level',chart_inside:'Inside (BME280)',chart_outside:'Outside (DHT22)',chart_empty:'Data accumulating... (updated every hour)',lbl_manual:'Manual',lbl_auto:'Auto',btn_on:'ON',btn_off:'OFF',set_tab_network:'Network',set_tab_mqtt:'MQTT',set_tab_creds:'Access',set_tab_ntp:'Time',set_tab_ui:'Interface',set_tab_ota:'Update',set_tab_system:'System',set_wifi:'Wi-Fi parameters',set_ssid:'SSID (network name)',set_wpass:'Wi-Fi password (empty = keep current)',set_wifi_scan:'Scan networks',set_forget_wifi:'Forget Wi-Fi',set_ip_mode:'IP mode',set_dhcp:'DHCP (auto)',set_static:'Static',set_dhcp_hint:'Controller receives address automatically from the router.',set_ip:'IP address',set_mask:'Subnet mask',set_gw:'Gateway',set_apply_net:'Apply & reboot',set_refresh:'Reload',set_reboot_warn:'\u26a0 Controller will reboot after apply (~10 sec)',set_mqtt_title:'MQTT (Home Assistant / Node-RED)',set_mqtt_enable:'Enable MQTT',set_mqtt_off:'no link',set_mqtt_disabled:'off',set_mqtt_on:'connected',set_mqtt_server:'MQTT server (IP or hostname)',set_mqtt_port:'Port',set_mqtt_user:'User (empty = no auth)',set_mqtt_pass:'MQTT password (empty = keep)',set_mqtt_apply:'Save MQTT',set_mqtt_resend:'Re-publish discovery',set_webaccess:'Web panel access',set_login:'Login',set_password:'New password',set_cur_password:'Current password',err_cur_password:'Enter the current password',err_cur_password_wrong:'The current password is wrong',set_save_creds:'Save credentials',set_creds_warn:'\u26a0 Other open panels will need to sign in again',set_ota_pass_title:'Network flashing from PlatformIO',set_ota_pass_hint:'Separate password for ArduinoOTA (8\u201331 characters, different from the panel password). Empty \u2014 the channel is off. Takes effect after a reboot.',set_ota_pass:'ArduinoOTA password',set_ota_pass_save:'Save',ota_pass_on:'Channel is on (password set)',ota_pass_off:'Channel is off',ota_pass_saved:'Saved. Takes effect after a reboot.',err_ota_pass_len:'Password: 8\u201331 characters, or empty to turn off',set_ntp_title:'Time synchronization (NTP)',set_ntp_server:'NTP server address',set_ntp_tz:'Timezone',set_ntp_interval:'Sync interval',set_ntp_apply:'Save NTP',set_ui_title:'Interface',set_theme:'Color theme',set_lang:'Interface language',theme_green:'\ud83c\udf40 Green',theme_beige:'\ud83c\udf3e Beige',theme_night:'\ud83c\udf19 Night',theme_win98:'\ud83d\udcbb Light gray',theme_blue:'\u2600 Sky blue',theme_brown:'\ud83c\udf42 Brown',set_ota_title:'Firmware update',set_ota_file:'File',set_ota_choose:'Choose file',set_ota_no_file:'No file chosen',set_ota_upload:'Upload',set_ota_current:'Current version:',ota_detected_container:'\u2713 Container {v} detected',ota_hint:'Upload an update file like greenhouse_vX.Y.bin. The controller will reboot automatically after update.',ota_detected_fw:'\u2713 Firmware update detected',ota_detected_fs:'\u2713 Filesystem update detected',ota_bad_name:'\u2717 Invalid filename. Use "greenhouse_vX.Y.bin" only.',ota_uploading:'Uploading...',ota_done:'Update complete. Rebooting...',ota_failed:'Update failed',ota_wait:'File uploaded. The controller is verifying and writing the update — do not close this page and do not cut the power.',ota_rebooting:'Update written, the controller is rebooting. This page will reload in {n} s — please wait.',set_sys_title:'System operations',tlm_title:'Anonymous statistics',tlm_enable:'Send anonymous statistics to the developer',tlm_on:'on',tlm_off:'off',tlm_text:'Off by default. Once an hour the controller sends one short message over HTTPS: the device ID, firmware version, install date, uptime, time zone, reason for the last start, minimum free memory, whether MQTT is on, and its address on your local network. The receiver sees your external IP address and derives the country from it. Never sent: Wi-Fi name and password, panel and MQTT passwords, sensor readings, relay states.',tlm_na:'This firmware build has no statistics receiver configured, so nothing is ever sent.',tlm_id:'Device ID',tlm_show:'Show exactly what is sent',tlm_last:'Last successful send',tlm_never:'none yet',tlm_err:'Last attempt failed, code',tlm_to:'Receiver',set_sys_reboot:'Reboot ESP32',set_sys_clear_hist:'Clear history',set_sys_factory:'Factory reset',set_sys_info:'Information',info_fw:'Firmware:',info_ip:'IP address:',info_mac:'MAC:',info_uptime:'Uptime:',info_heap:'Free heap:',info_rssi:'Wi-Fi RSSI:',about_title:'Smart Greenhouse',about_version:'Version',about_desc:'Greenhouse automation: irrigation, ventilation, humidification, water tank.<br>Real-time monitoring (BME280, DHT22, BH1750, AJ-SR04M).',about_author:'\u00a9 Sergey Saidov',offline_title:'Controller unavailable',offline_sub:'No connection to ESP32. Page loaded from cache.<br>Check controller power.',offline_btn:'\u21bb Reload page',login_title:'Sign in',login_user:'Login',login_pass:'Password',login_btn:'Sign in',ws_connected:'Connected',ws_connecting:'Connecting...',ws_offline:'No connection',ws_auth_fail:'Invalid credentials',ws_busy:'Too many open panels — retrying',toast_saved:'Saved',pw_default_warn:'Default login and password (admin / admin) are in use. Anyone on your network can control the greenhouse.',pw_default_btn:'Change',set_tab_auto:'Automation',btn_configure:'Configure',cfg_sub:'Automation settings',cfg_all:'All automation settings',sum_vents:'Open at {o}\u00a0\u00b0C, close at {c}\u00a0\u00b0C',sum_humid:'On below {l} %, off at {h} %',sum_pump:'On at {l} %, off at {h} %',alert_rain:'Rain \u2014 vents closed',alert_sensor:'Sensor fault: {n}',toast_no_conn:'No connection',toast_confirm_logout:'Logout?',toast_confirm_reboot:'Reboot ESP32?',toast_confirm_clear:'Clear all history?',toast_confirm_factory:'FACTORY RESET. Greenhouse settings will be lost (Wi-Fi and the panel password are kept). Continue?',set_sys_factory_hint:'Factory reset does not touch Wi-Fi or the panel password. To change the Wi-Fi network, use Settings → Network → Forget Wi-Fi.',toast_confirm_factory2:'Are you sure? This cannot be undone.',toast_confirm_forget_wifi:'Delete Wi-Fi credentials and reboot into AP mode?',toast_confirm_calibrate:'Run vent calibration? The vent will close completely.',toast_calib_started:'Calibration started',toast_fault_reset:'Fault reset',toast_discovery_sent:'Discovery published',lbl_homing:'Position reset',lbl_homing_hint:'Closes the sash to the limit switch and resets the counter to zero. Use if the shown position no longer matches reality.',btn_home_top:'Reset upper',btn_home_bottom:'Reset lower',lbl_homing_run:'Running\u2026',btn_home_cancel:'Stop',toast_confirm_home:'The sash will close completely and its position counter will reset to zero. Continue?',toast_home_started:'Position reset started',err_fill_all:'Fill in all fields',err_close_lt_open:'\u201cClose\u201d must be below \u201cOpen\u201d',err_calib_range:'Travel time: 1\u2013120 s',msg_no_changes:'Nothing changed',err_bad_thresholds:'Check the thresholds',err_on_lt_off:'\u201cOn\u201d must be below \u201cOff\u201d',err_creds_len:'Login from 1 character, password from 8',err_default_creds:'Change the default password (admin / admin) first — until then the controller accepts no changes or updates.',err_generic:'Error',msg_rebooting:'Rebooting\u2026',msg_resetting:'Resetting to factory defaults\u2026',err_load_settings:'Could not load settings',err_load_network:'Could not load network settings',err_load_mqtt:'Could not load MQTT settings',wifi_scanning:'Scanning\u2026',wifi_none:'No networks found',wifi_scan_failed:'Scan failed',},ru:{nav_control:'Управление',nav_settings:'Настройки',nav_about:'О программе',nav_history:'История',nav_events:'Журнал событий',ev_hint:'До 32 важных и 48 обычных событий: тревоги не вытесняются рутиной. Обновляется раз в 10 секунд, пока страница открыта. Период — только фильтр показа, записи не удаляются.',ev_all:'Все',ev_p1:'24 часа',ev_p7:'7 дней',ev_p30:'30 дней',ev_alarms:'Тревоги',ev_water:'Вода',ev_vents:'Форточки',ev_sys:'Система',ev_empty:'Событий пока нет',ev_today:'Сегодня',ev_yesterday:'Вчера',ev_notime:'До синхронизации часов',ev_boot:'Запуск контроллера — {r}',ev_on:'{n}: включён',ev_off:'{n}: выключен',ev_vent:'{n}: {a} → {b} %',ev_upper:'Верхняя форточка',ev_lower:'Нижняя форточка',ev_mode_auto:'{n}: режим «Авто»',ev_mode_manual:'{n}: режим «Ручной»',ev_pump_fault:'Авария насоса',ev_pump_ok:'Авария насоса снята',ev_sens_fail:'Датчик {n}: отказ',ev_sens_ok:'Датчик {n}: снова работает',ev_rain_on:'Начался дождь',ev_rain_off:'Дождь закончился',ev_wifi_off:'Wi-Fi: связь потеряна',ev_wifi_on:'Wi-Fi: подключено',ev_auth:'Неудачные попытки входа: {n}',ev_set:'Изменены настройки: {n}',ev_ota_ok:'Обновление прошивки установлено',ev_ota_fail:'Обновление прошивки не удалось',ev_reboot_panel:'Перезагрузка из панели',ev_reboot_plan:'Плановая перезагрузка',ev_hist_cleared:'История очищена',evs_0:'пороги',evs_1:'таймеры полива',evs_2:'накопительная ёмкость',evs_3:'сеть',evs_4:'MQTT',evs_5:'логин и пароль панели',evs_6:'время (NTP)',evs_7:'калибровка форточек',evs_8:'сброс к заводским',evn_irr1:'Полив 1',evn_irr2:'Полив 2',evn_irr3:'Полив 3',evn_humid:'Увлажнитель',evn_pump:'Насос ёмкости',evm_0:'Форточки',evm_1:'Полив',evm_2:'Увлажнитель',evm_3:'Насос ёмкости',nav_system:'Состояние системы',hist_stats:'За выбранный период',st_series:'Ряд',st_min:'Мин',st_avg:'Среднее',st_max:'Макс',st_last:'Последнее',st_none:'За этот период данных нет',sys_hint:'Обновляется раз в 5 секунд, пока страница открыта.',sys_run:'Работа',sys_fw:'Прошивка',sys_uptime:'Время работы',sys_reset:'Последний запуск',sys_time:'Часы (NTP)',sys_synced:'синхронизированы',sys_not_synced:'не синхронизированы',sys_mem:'Память',sys_heap:'Свободно',sys_heap_min:'Минимум с запуска',sys_largest:'Крупнейший блок',sys_nvs:'Память настроек (NVS)',sys_nvs_val:'занято {u} из {t} записей',sys_nvs_wr:'Записей в NVS с запуска',sys_net:'Сеть',sys_ssid:'Сеть Wi-Fi',sys_signal:'Сигнал',sys_ip:'IP-адрес',sys_mac:'MAC',sys_mqtt:'MQTT',sys_on:'подключён',sys_off:'нет связи',sys_rescue:'Аварийная точка доступа',sys_rescue_on:'включена',sys_rescue_off:'выключена',sys_sensors:'Датчики',sys_ok:'работает',sys_fail:'неисправен',sys_stack:'Запас стека задач',sys_stack_core1:'Управляющий цикл',sys_stack_loop:'Основной цикл (MQTT, OTA)',sys_stack_tcp:'Веб-сервер',sys_stack_wifi:'Контроль Wi-Fi',sys_absent:'не запущена',rr_POWERON:'включение питания',rr_SW:'программный перезапуск (обновление, кнопка, план)',rr_EXT:'кнопка сброса',rr_PANIC:'сбой программы',rr_INT_WDT:'сторожевой таймер (зависание)',rr_TASK_WDT:'сторожевой таймер (зависание)',rr_WDT:'сторожевой таймер (зависание)',rr_BROWNOUT:'просадка питания',rr_DEEPSLEEP:'выход из сна',rr_UNKNOWN:'неизвестно',up_d:'д',btn_logout:'Выход',lbl_time:'ВРЕМЯ',lbl_uptime:'В РАБОТЕ',lbl_sensors:'Датчики',lbl_temp:'Температура',lbl_hum:'Влажность',lbl_press:'Давление',lux_unit:'лк',lbl_vents:'Форточки',lbl_upper_travel:'Верхняя (42 сек)',lbl_lower_travel:'Нижняя (24 сек)',lbl_upper:'Верхняя форточка',lbl_lower:'Нижняя форточка',lbl_closed:'Закрыта',lbl_opened:'Открыта',btn_calibrate:'Калибровка',lbl_travel_sec:'Время хода, сек',btn_vent_open:'ОТКРЫТЬ',btn_vent_close:'ЗАКРЫТЬ',lbl_temp_open:'Открыть при, °C',lbl_temp_close:'Закрыть при, °C',lbl_calibration:'Калибровка (полное время хода)',lbl_calib_top:'Верхняя, сек',lbl_calib_bottom:'Нижняя, сек',lbl_rain:'Дождь',vstate_idle:'Ожидание',vstate_open_top:'Открывается верхняя...',vstate_open_bottom:'Открывается нижняя...',vstate_close_top:'Закрывается верхняя...',vstate_close_bottom:'Закрывается нижняя...',vstate_pause:'Пауза (замер t)...',vstate_rain:'Дождь — экстренное закрытие',lbl_vent_open_t:'Открыть при, \u00b0C',lbl_vent_close_t:'Закрыть при, \u00b0C',lbl_irrigation:'Полив',lbl_irr1:'Полив 1',lbl_irr2:'Полив 2',lbl_irr3:'Полив 3',lbl_start:'Время старта',lbl_dur:'Длит., мин',lbl_interval:'Интервал, ч',timer_active:'АКТИВИРОВАН',lbl_apply:'Применить',lbl_apply_irr:'\u2699 Применить настройки таймеров',lbl_apply_hum:'Применить',lbl_apply_water:'Применить',lbl_humidifier:'Увлажнитель',lbl_state:'Состояние',cd_units:'до запуска: сут ч:мин',lbl_humid_on:'Включён',lbl_humid_off:'Отключён',lbl_hum_off_pct:'Выкл при влажн., %',lbl_hum_on_pct:'Вкл при влажн., %',lbl_water_tank:'Накопительная ёмкость',lbl_pump_state:'Насос',lbl_pump_on:'Включён',lbl_pump_off:'Выключен',lbl_pump_fault:'Авария насоса \u2014 проверьте датчик',btn_reset_fault:'Сбросить',lbl_pump_on_pct:'Вкл при, %',lbl_pump_off_pct:'Выкл при, %',lbl_tank_depth:'Глубина, см',lbl_pump_timeout:'Таймаут, мин',lbl_history:'История',hist_day:'Сутки',hist_week:'Неделя',tab_temp:'Температура',tab_hum:'Влажность',tab_press:'Давление',tab_water:'Уровень воды',chart_inside:'Внутри (BME280)',chart_outside:'Снаружи (DHT22)',chart_empty:'Данные накапливаются... (обновление каждый час)',lbl_manual:'Ручной',lbl_auto:'Авто',btn_on:'ВКЛ',btn_off:'ВЫКЛ',set_tab_network:'Сеть',set_tab_mqtt:'MQTT',set_tab_creds:'Доступ',set_tab_ntp:'Время',set_tab_ui:'Интерфейс',set_tab_ota:'Обновление',set_tab_system:'Система',set_wifi:'Параметры WiFi',set_ssid:'SSID (имя сети)',set_wpass:'Пароль WiFi (пусто = без изменений)',set_wifi_scan:'Сканировать сети',set_forget_wifi:'Забыть Wi-Fi',set_ip_mode:'Режим IP-адреса',set_dhcp:'DHCP (авто)',set_static:'Статический',set_dhcp_hint:'Контроллер получает адрес автоматически от роутера.',set_ip:'IP-адрес',set_mask:'Маска',set_gw:'Шлюз',set_apply_net:'Применить и перезагрузить',set_refresh:'Обновить',set_reboot_warn:'\u26a0 После применения контроллер перезагрузится (~10 сек)',set_mqtt_title:'MQTT (Home Assistant / Node-RED)',set_mqtt_enable:'Включить MQTT',set_mqtt_off:'нет связи',set_mqtt_disabled:'отключён',set_mqtt_on:'подключён',set_mqtt_server:'MQTT сервер (IP или hostname)',set_mqtt_port:'Порт',set_mqtt_user:'Логин (пусто = без авторизации)',set_mqtt_pass:'Пароль MQTT (пусто = без изменений)',set_mqtt_apply:'Сохранить MQTT',set_mqtt_resend:'Пересоздать discovery',set_webaccess:'Доступ к веб-панели',set_login:'Логин',set_password:'Новый пароль',set_cur_password:'Текущий пароль',err_cur_password:'Введите текущий пароль',err_cur_password_wrong:'Текущий пароль неверный',set_save_creds:'Сохранить доступ',set_creds_warn:'\u26a0 В других открытых панелях потребуется войти заново',set_ota_pass_title:'Прошивка по сети из PlatformIO',set_ota_pass_hint:'Отдельный пароль ArduinoOTA (8–31 символ, не совпадает с паролем панели). Пусто — канал выключен. Действует после перезагрузки.',set_ota_pass:'Пароль ArduinoOTA',set_ota_pass_save:'Сохранить',ota_pass_on:'Канал включён (пароль задан)',ota_pass_off:'Канал выключен',ota_pass_saved:'Сохранено. Вступит в силу после перезагрузки.',err_ota_pass_len:'Пароль: 8–31 символ или пусто, чтобы выключить',set_ntp_title:'Синхронизация времени (NTP)',set_ntp_server:'Адрес NTP-сервера',set_ntp_tz:'Часовой пояс',set_ntp_interval:'Интервал синхронизации',set_ntp_apply:'Сохранить NTP',set_ui_title:'Интерфейс',set_theme:'Схема оформления',set_lang:'Язык интерфейса',theme_green:'\ud83c\udf40 Зелёная',theme_beige:'\ud83c\udf3e Бежевая',theme_night:'\ud83c\udf19 Ночная',theme_win98:'\ud83d\udcbb Светло-серая',theme_blue:'\u2600 Бледно-голубая',theme_brown:'\ud83c\udf42 Коричневая',set_ota_title:'Обновление прошивки',set_ota_file:'Файл',set_ota_choose:'Выбрать файл',set_ota_no_file:'Файл не выбран',set_ota_upload:'Загрузить',set_ota_current:'Текущая версия:',ota_detected_container:'\u2713 Контейнер {v} обнаружен',ota_hint:'Загрузите файл обновления вида greenhouse_vX.Y.bin. Контроллер автоматически перезагрузится после обновления.',ota_detected_fw:'\u2713 Обнаружено обновление прошивки',ota_detected_fs:'\u2713 Обнаружено обновление файловой системы',ota_bad_name:'\u2717 Недопустимое имя файла. Используйте только "greenhouse_vX.Y.bin".',ota_uploading:'Загрузка...',ota_done:'Обновление завершено. Перезагрузка...',ota_failed:'Ошибка обновления',ota_wait:'Файл загружен. Контроллер проверяет и записывает обновление — не закрывайте страницу и не отключайте питание.',ota_rebooting:'Обновление записано, контроллер перезагружается. Страница обновится сама через {n} с — подождите.',set_sys_title:'Системные операции',tlm_title:'Анонимная статистика',tlm_enable:'Отправлять анонимную статистику разработчику',tlm_on:'включена',tlm_off:'выключена',tlm_text:'По умолчанию выключено. Раз в час контроллер шлёт по HTTPS одно короткое сообщение: идентификатор устройства, версию прошивки, дату установки, время работы, часовой пояс, причину последнего запуска, минимум свободной памяти, включён ли MQTT и свой адрес в локальной сети. Приёмник видит ваш внешний IP-адрес и по нему определяет страну. Никогда не отправляются: имя и пароль Wi-Fi, пароли панели и MQTT, показания датчиков, состояние реле.',tlm_na:'В этой сборке прошивки не задан приёмник статистики, поэтому ничего не отправляется.',tlm_id:'Идентификатор устройства',tlm_show:'Показать, что именно отправляется',tlm_last:'Последняя удачная отправка',tlm_never:'пока не было',tlm_err:'Последняя попытка не удалась, код',tlm_to:'Приёмник',set_sys_reboot:'Перезагрузить ESP32',set_sys_clear_hist:'Очистить историю',set_sys_factory:'Сброс к заводским настройкам',set_sys_info:'Информация',info_fw:'Прошивка:',info_ip:'IP адрес:',info_mac:'MAC:',info_uptime:'Время работы:',info_heap:'Свободная память:',info_rssi:'Уровень Wi-Fi (RSSI):',about_title:'Умная теплица',about_version:'Версия',about_desc:'Система автоматизации теплицы: полив, вентиляция, увлажнение, накопительная ёмкость.<br>Мониторинг в реальном времени (BME280, DHT22, BH1750, AJ-SR04M).',about_author:'\u00a9 Сергей Саидов',offline_title:'Контроллер недоступен',offline_sub:'Нет соединения с ESP32. Страница загружена из кэша.<br>Проверьте питание контроллера.',offline_btn:'\u21bb Обновить страницу',login_title:'Вход в систему',login_user:'Логин',login_pass:'Пароль',login_btn:'Войти',ws_connected:'Подключено',ws_connecting:'Подключение...',ws_offline:'Нет соединения',ws_auth_fail:'Неверный логин или пароль',ws_busy:'Открыто слишком много панелей — повтор',toast_saved:'Сохранено',pw_default_warn:'Используются логин и пароль по умолчанию (admin / admin). Любой в вашей сети может управлять теплицей.',pw_default_btn:'Сменить',set_tab_auto:'Автоматика',btn_configure:'Настроить',cfg_sub:'Настройки автоматики',cfg_all:'Все настройки автоматики',sum_vents:'Открыть при {o}\u00a0\u00b0C, закрыть при {c}\u00a0\u00b0C',sum_humid:'Вкл. ниже {l} %, выкл. при {h} %',sum_pump:'Вкл. при {l} %, выкл. при {h} %',alert_rain:'Дождь \u2014 форточки закрыты',alert_sensor:'Неисправен датчик: {n}',toast_no_conn:'Нет соединения',toast_confirm_logout:'Выйти?',toast_confirm_reboot:'Перезагрузить ESP32?',toast_confirm_clear:'Очистить всю историю?',toast_confirm_factory:'СБРОС К ЗАВОДСКИМ. Настройки теплицы будут потеряны (Wi-Fi и пароль панели сохранятся). Продолжить?',set_sys_factory_hint:'Сброс к заводским не трогает Wi-Fi и пароль панели. Чтобы сменить сеть Wi-Fi, используйте «Настройки → Сеть → Забыть Wi-Fi».',toast_confirm_factory2:'Точно? Это действие нельзя отменить.',toast_confirm_forget_wifi:'Удалить Wi-Fi креды и перезагрузиться в режим AP?',toast_confirm_calibrate:'Запустить калибровку форточки? Форточка полностью закроется.',toast_calib_started:'Калибровка запущена',toast_fault_reset:'Авария сброшена',toast_discovery_sent:'Discovery отправлен',lbl_homing:'Сброс позиции',lbl_homing_hint:'Закрывает створку до концевика и обнуляет счётчик. Применяйте, если показанная позиция разошлась с реальностью.',btn_home_top:'Сбросить верхнюю',btn_home_bottom:'Сбросить нижнюю',lbl_homing_run:'Идёт прогон…',btn_home_cancel:'Остановить',toast_confirm_home:'Створка полностью закроется, а счётчик позиции обнулится. Продолжить?',toast_home_started:'Сброс позиции запущен',err_fill_all:'Заполните все поля',err_close_lt_open:'«Закрыть» должно быть меньше «Открыть»',err_calib_range:'Время хода: 1–120 с',msg_no_changes:'Изменений нет',err_bad_thresholds:'Проверьте пороги',err_on_lt_off:'«Вкл» должно быть меньше «Выкл»',err_creds_len:'Логин от 1 символа, пароль от 8',err_default_creds:'Сначала смените заводской пароль (admin / admin) — до этого контроллер не примет ни изменений, ни обновлений.',err_generic:'Ошибка',msg_rebooting:'Перезагрузка…',msg_resetting:'Сброс к заводским настройкам…',err_load_settings:'Не удалось загрузить настройки',err_load_network:'Не удалось загрузить настройки сети',err_load_mqtt:'Не удалось загрузить настройки MQTT',wifi_scanning:'Поиск сетей…',wifi_none:'Сети не найдены',wifi_scan_failed:'Не удалось выполнить поиск',}};let currentLang='en';
// Кеш последних значений формы форточек — для отправки только изменённых полей
var _lastFormVent={vTOpen:null,vTClose:null,vuTravel:null,vlTravel:null};function i18n(k){var d=GH_I18N[currentLang]||GH_I18N.en;var v=d[k];if(v==null)v=GH_I18N.en[k];return v!=null?v:k;}
const WS_URL='ws://'+location.hostname+'/ws';const RECONNECT_MS=3000,OFFLINE_AFTER_N=4;var _ss=window.sessionStorage;var _authUser=_ss.getItem('gh_u')||'';
// Пароль в браузере больше не хранится: при входе он один раз уходит в POST /api/login,
// дальше — токен сессии. Пароль, оставшийся в sessionStorage от прежней версии, стираем.
var _authToken=_ss.getItem('gh_t')||'';try{_ss.removeItem('gh_p');}catch(e){}
// Заводские admin/admin (флаг приходит от контроллера): баннер и переход к смене пароля.
var _credsDefault=false;let ws=null,wsTimer=null,wsReconnectCount=0,statePoll=null,_wsAuthed=false;let histData=[],activeChart='temp',_btnLastMs={};const $=function(id){return document.getElementById(id);};const pad2=function(n){return String(n).padStart(2,'0');};function setVal(id,v){var e=$(id);if(e!=null&&v!==undefined&&v!==null)e.value=v;}
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
function toast(msg,type){var c=$('toastContainer');if(!c)return;var el=document.createElement('div');el.className='toast '+(type||'info');if(type==='error')el.setAttribute('role','alert');el.textContent=msg;c.appendChild(el);setTimeout(function(){el.remove();},4000);}
function showLoginScreen(){var ls=$('loginScreen'),as=$('appShell');if(ls)ls.style.display='flex';if(as)as.style.display='none';}
function showAppShell(){var ls=$('loginScreen'),as=$('appShell');if(ls)ls.style.display='none';if(as)as.style.display='flex';var pb=$('pwBanner');if(pb)pb.style.display=_credsDefault?'flex':'none';}
// v6.3: btoa() принимает только символы Latin-1 и на кириллице в логине или
// пароле бросает исключение — кнопка входа оставалась заблокированной, зайти
// в панель было невозможно. Контроллер хранит логин и пароль в UTF-8 (так их
// присылает JSON) и сравнивает base64 тех же байт, поэтому кодируем в UTF-8.
function b64Auth(u,p){var b=new TextEncoder().encode(u+':'+p),s='';for(var i=0;i<b.length;i++)s+=String.fromCharCode(b[i]);return btoa(s);}
function doLogin(){var u=($('loginUser').value||'').trim();var p=$('loginPass').value||'';var err=$('loginError');if(u.length<1||p.length<1){if(err){err.textContent=i18n('login_user')+' / '+i18n('login_pass');err.style.display='block';}
return;}
if(err)err.style.display='none';var btn=$('btnLogin');if(btn)btn.disabled=true;var b64=b64Auth(u,p);fetch('/api/login',{method:'POST',headers:{'Authorization':'Basic '+b64}}).then(function(r){if(btn)btn.disabled=false;if(r.ok){return r.json().then(function(d){if(!d.token)throw new Error('no token');_authUser=u;_authToken=d.token;_ss.setItem('gh_u',u);_ss.setItem('gh_t',d.token);$('loginPass').value='';_credsDefault=!!d.credsDefault;showAppShell();wsConnect();loadAllSettings();if(_credsDefault)goChangeCreds();});}else{if(err){err.textContent=i18n('ws_auth_fail');err.style.display='block';}
$('loginPass').value='';$('loginPass').focus();}}).catch(function(){if(btn)btn.disabled=false;if(err){err.textContent=i18n('toast_no_conn');err.style.display='block';}});}
function loginKeydown(e){if(e.key==='Enter')doLogin();}
function doLogout(){if(!confirm(i18n('toast_confirm_logout')))return;
// Сессию закрываем и на контроллере: токен из этой вкладки больше ничего не откроет.
if(_authToken)fetch('/api/logout',{method:'POST',headers:authHeaders()}).catch(function(){});
var tk=_authToken;_authUser='';_authToken='';_ss.removeItem('gh_u');_ss.removeItem('gh_t');if(ws&&ws.readyState===WebSocket.OPEN)ws.close();showLoginScreen();return tk;}
window.doLogout=doLogout;function authHeaders(extra){var h=Object.assign({'Authorization':'Bearer '+_authToken},extra||{});return h;}
// Заводской пароль: открыть «Настройки → Доступ» — до смены контроллер ничего не даст менять.
function goChangeCreds(){showPage('settings');showSettingsTab('creds');toast(i18n('err_default_creds'),'warning');var u=$('wbUser');if(u)u.focus();}
window.goChangeCreds=goChangeCreds;
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
el.textContent=v;});var langSel=$('langSel');if(langSel)langSel.value=lang;document.querySelectorAll('.relay-btn').forEach(function(b){b.textContent=b.classList.contains('on')?i18n('btn_on'):i18n('btn_off');});_alertSig='';syncCfgViews();if(typeof updateHistStats==='function')updateHistStats(activeChart);}
async function applyLangAndSave(lang){applyLang(lang);try{localStorage.setItem(LANG_KEY,lang);}catch(e){}
if(_authUser){try{var theme=document.documentElement.dataset.theme||'';await fetchJson('/api/ui',{method:'POST',headers:authHeaders({'Content-Type':'application/json'}),body:JSON.stringify({theme:theme,lang:lang})});}catch(e){console.warn('[UI] lang save failed',e);}}}
window.applyLang=applyLang;window.applyLangAndSave=applyLangAndSave;// v6.3: страниц четыре. «История» — тот же график, что на главном экране: карточка на время
// показа переносится на страницу и растягивается (один canvas, одни обработчики касания).
var PAGES={dashboard:'pageDashboard',history:'pageHistory',events:'pageEvents',system:'pageSystem',settings:'pageSettings'};
var _chartHome=null,_sysTimer=null;
function moveChart(toHist){var c=document.querySelector('.chart-card'),slot=$('histPageSlot');if(!c||!slot)return;if(!_chartHome){_chartHome=document.createComment('chart-home');c.parentNode.insertBefore(_chartHome,c);}
if(toHist){if(c.parentNode!==slot)slot.appendChild(c);c.classList.add('chart-big');}else if(c.parentNode===slot){_chartHome.parentNode.insertBefore(c,_chartHome.nextSibling);c.classList.remove('chart-big');}}
function sysPoll(on){clearInterval(_sysTimer);_sysTimer=null;if(!on)return;var ask=function(){if(ws&&ws.readyState===WebSocket.OPEN&&_wsAuthed)ws.send(JSON.stringify({cmd:'get_sysinfo'}));};ask();_sysTimer=setInterval(ask,5000);}
function showPage(name){if(!PAGES[name])name='dashboard';Object.keys(PAGES).forEach(function(k){var e=$(PAGES[k]);if(e){e.classList.remove('active');e.style.display='none';}});var el=$(PAGES[name]);if(!el)return;el.classList.add('active');
moveChart(name==='history');sysPoll(name==='system');evPoll(name==='events');
if(name==='settings'){el.style.display='block';loadAllSettings();setTimeout(updateTabsHint,0);}
else if(name==='dashboard'){el.style.display='grid';setTimeout(function(){redrawAll();drawHistChart(activeChart);},50);}
else{el.style.display='block';if(name==='history')setTimeout(function(){drawHistChart(activeChart);},30);}
document.querySelectorAll('.sb-link[data-page]').forEach(function(a){a.classList.toggle('sb-active',a.dataset.page===name);});closeSidebar();}
window.showPage=showPage;function showSettingsTab(tab){document.querySelectorAll('.set-tab').forEach(function(b){b.classList.remove('active');});var matched=null;document.querySelectorAll('.set-tab').forEach(function(b){if(b.getAttribute('onclick')&&b.getAttribute('onclick').indexOf("'"+tab+"'")>=0)matched=b;});if(matched)matched.classList.add('active');document.querySelectorAll('.set-pane').forEach(function(p){p.classList.toggle('active',p.dataset.pane===tab);});if(matched&&matched.scrollIntoView)matched.scrollIntoView({block:'nearest',inline:'nearest'});if(tab==='system')loadTelemetry();updateTabsHint();}
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
if(!_authToken)return;   // нечем авторизоваться

try{ws=new WebSocket(WS_URL);}catch(e){console.error('[WS] construct failed',e);wsTimer=setTimeout(wsConnect,RECONNECT_MS);return;}
ws.onopen=function(){clearTimeout(wsTimer);wsReconnectCount=0;hideOfflineOverlay();
// До подтверждения авторизации контроллер не отдаёт ни состояния,
// ни истории, поэтому опрос запускаем только по ответу на auth.
setWsUI('authing',i18n('ws_connecting'));
_wsAuthed=false;
ws.send(JSON.stringify({cmd:'auth',token:_authToken}));};ws.onclose=function(){_wsAuthed=false;stopStatePoll();setWsUI('offline',i18n('ws_offline'));wsReconnectCount++;if(wsReconnectCount>=OFFLINE_AFTER_N)showOfflineOverlay();// v6.1: переподключаемся ТОЛЬКО если есть с чем авторизоваться.
// Иначе после отказа авторизации получался бесконечный цикл: креды
// очищены, пользователь на экране входа, а сокет каждые 3 секунды
// открывался заново и слал заведомо неверный auth.
if(_authToken&&!document.hidden)wsTimer=setTimeout(wsConnect,RECONNECT_MS);};ws.onerror=function(e){console.error('[WS]',e);};ws.onmessage=function(e){try{onMessage(JSON.parse(e.data));}catch(err){console.error('[WS parse]',err);}};}
// v6.1: контроллер больше не рассылает состояние сам (AsyncWebSocket не
// потокобезопасен — см. ws_handler.cpp). Панель сама опрашивает раз в
// секунду; ответ формируется в задаче AsyncTCP, гонки нет.
// Опрос идёт мимо wsSend, чтобы не мигать индикатором активности.
function startStatePoll(){stopStatePoll();statePoll=setInterval(function(){if(ws&&ws.readyState===WebSocket.OPEN&&_wsAuthed)ws.send(JSON.stringify({cmd:'get_state'}));},1000);}
function stopStatePoll(){if(statePoll){clearInterval(statePoll);statePoll=null;}}
// Вкладка в фоне дольше 30 с закрывает сокет: у контроллера 8 слотов авторизации,
// и забытые вкладки не должны занимать их и заставлять его собирать снимок каждую
// секунду. Вернулись на вкладку — переподключаемся сразу.
var _hiddenTimer=null;
function onVisibility(){clearTimeout(_hiddenTimer);_hiddenTimer=null;
if(document.hidden){_hiddenTimer=setTimeout(function(){if(document.hidden&&ws&&ws.readyState===WebSocket.OPEN)ws.close();},30000);}
else if(_authToken&&(!ws||ws.readyState===WebSocket.CLOSED)){clearTimeout(wsTimer);wsConnect();}}
function wsSend(o){if(!ws||ws.readyState!==WebSocket.OPEN||!_wsAuthed){toast(i18n('toast_no_conn'),'error');return false;}
ws.send(JSON.stringify(o));var sp=$('wsSpinner');if(sp){sp.classList.add('show');setTimeout(function(){sp.classList.remove('show');},1200);}
return true;}
function setWsUI(cls,label){var d=$('wsStatus'),l=$('wsLabel');if(d){d.className='ws-dot '+cls;d.title=label;}
if(l){l.textContent=label;l.classList.remove('blink-offline');if(cls==='online'){l.style.color='#00e676';}
else if(cls==='offline'){l.style.color='#f44336';l.classList.add('blink-offline');}
else{l.style.color='';}}}
function onMessage(m){if(m.type==='state')renderState(m);else if(m.type==='history')renderHistory(m.points||histColumns(m));else if(m.type==='sysinfo')renderSysInfo(m);else if(m.type==='events')renderEvents(m);else if(m.type==='ack'){
  if(m.cmd==='auth'){
    if(m.ok){_wsAuthed=true;setWsUI('online',i18n('ws_connected'));startStatePoll();
      setTimeout(function(){wsSend({cmd:'get_history'});},600);}
    // Нет свободного слота на контроллере — пароль верный, разлогинивать нельзя:
    // контроллер закроет сокет, onclose переподключится через RECONNECT_MS.
    else if(m.error==='too many connections'){_wsAuthed=false;setWsUI('offline',i18n('ws_busy'));}
    else{_wsAuthed=false;setWsUI('offline',i18n('ws_auth_fail'));onAuthLost();}
    return;}
  if(m.cmd==='clear_history'&&m.ok){_histAll=[];histData=[];_chartSel=null;drawHistChart(activeChart);updateHistStats(activeChart);}
  saveAck(m);
  if(!m.ok){_lastFormVent.vTOpen=_lastFormVent.vTClose=_lastFormVent.vuTravel=_lastFormVent.vlTravel=null;
    if(m.error==='default_credentials'){_credsDefault=true;showAppShell();toast(i18n('err_default_creds'),'error');}
    else toast(i18n('err_generic')+': '+(m.error||m.cmd),'error');}}}
// v6.3: «Сохранено» показываем только после подтверждения контроллера (ack), а не при отправке.
// Команды одного нажатия (одного синхронного обработчика) — одно задание со своим таймаутом;
// зелёный тост — когда подтверждены все команды задания без ошибок. Контроллер отвечает
// на команды клиента по порядку, поэтому ack сопоставляются с самым старым заданием, ждущим
// такую команду. После успеха настройки перечитываются: сводка порогов и зоны шкал на
// главном экране берутся из /api/settings.
var _saveJobs=[],_saveOpen=null;
// Поля форм, которые пишет каждая команда. После успеха задание снимает с них отметку
// «правится» (data-dirty); остальные несохранённые правки перечитывание не трогает.
var SAVE_FIELDS={set_thresholds:['vTOpen','vTClose','hmL','hmH'],vent_set_calib:['vuTravel','vlTravel'],set_water_tank:['wtLP','wtHP','wtDp','wtTm'],set_timer:['start0','dur0','int0','start1','dur1','int1','start2','dur2','int2']};
function clearDirty(cmds){cmds.forEach(function(c){(SAVE_FIELDS[c]||[]).forEach(function(id){var e=$(id);if(e)delete e.dataset.dirty;});});}
function markDirty(e){var t=e.target;if(!t||!t.id)return;for(var k in SAVE_FIELDS){if(SAVE_FIELDS[k].indexOf(t.id)>=0){t.dataset.dirty='1';return;}}}
function savingSend(o){if(!wsSend(o))return false;if(!_saveOpen){var job={left:[],cmds:[],failed:false};job.t=setTimeout(function(){var i=_saveJobs.indexOf(job);if(i>=0)_saveJobs.splice(i,1);toast(i18n('toast_no_conn'),'error');},5000);_saveJobs.push(job);_saveOpen=job;setTimeout(function(){_saveOpen=null;},0);}_saveOpen.left.push(o.cmd);_saveOpen.cmds.push(o.cmd);return true;}
function saveAck(m){for(var j=0;j<_saveJobs.length;j++){var job=_saveJobs[j],si=job.left.indexOf(m.cmd);if(si<0)continue;job.left.splice(si,1);if(!m.ok)job.failed=true;if(!job.left.length){clearTimeout(job.t);_saveJobs.splice(j,1);if(!job.failed){toast(i18n('toast_saved'),'success');clearDirty(job.cmds);loadAllSettings();closeCfgSheet();}}return;}}
function fmtUptime(sec){sec=sec|0;var d=Math.floor(sec/86400),h=Math.floor((sec%86400)/3600),m=Math.floor((sec%3600)/60);return d+' '+i18n('up_d')+' '+pad2(h)+':'+pad2(m);}
function fmtKb(b){return (b/1024).toFixed(1)+' KB';}
// Карточка «ключ — значение»: значения только через textContent.
function sysCard(title,rows){var c=document.createElement('section');c.className='card sys-card';var h=document.createElement('div');h.className='card-title';h.textContent=title;c.appendChild(h);
rows.forEach(function(r){if(!r)return;var row=document.createElement('div');row.className='sys-row';var k=document.createElement('span');k.className='sys-k';k.textContent=r[0];var v=document.createElement('span');v.className='sys-v'+(r[2]?' '+r[2]:'');v.textContent=r[1];row.appendChild(k);row.appendChild(v);
if(r[3]!=null){var bar=document.createElement('div');bar.className='sys-bar';var fill=document.createElement('i');fill.style.width=Math.max(0,Math.min(100,r[3]))+'%';if(r[2])fill.className=r[2];bar.appendChild(fill);row.appendChild(bar);}c.appendChild(row);});return c;}
function renderSysInfo(m){var g=$('sysGrid');if(!g)return;g.textContent='';
var badReset=['PANIC','INT_WDT','TASK_WDT','WDT','BROWNOUT'].indexOf(m.reset)>=0;
var stk=function(lbl,v){return [lbl,v?v+' B':i18n('sys_absent'),!v?'':v<512?'bad':v<1024?'warn':'good'];};
var nvs=m.nvs||{},nvsPct=nvs.total?nvs.used*100/nvs.total:null,w=m.wifi||{},sn=m.sensors||{},st=m.stack||{};
var lgPct=m.heap?m.largest*100/m.heap:100;
g.appendChild(sysCard(i18n('sys_run'),[[i18n('sys_fw'),m.fw||'—'],[i18n('sys_uptime'),fmtUptime(m.uptime)],[i18n('sys_reset'),i18n('rr_'+(m.reset||'UNKNOWN')),badReset?'bad':''],[i18n('sys_time'),m.timeSync?i18n('sys_synced'):i18n('sys_not_synced'),m.timeSync?'good':'warn']]));
g.appendChild(sysCard(i18n('sys_mem'),[[i18n('sys_heap'),fmtKb(m.heap||0)],[i18n('sys_heap_min'),fmtKb(m.heapMin||0),m.heapMin<20480?'bad':m.heapMin<40960?'warn':''],[i18n('sys_largest'),fmtKb(m.largest||0),lgPct<25?'warn':''],
 nvs.total?[i18n('sys_nvs'),i18n('sys_nvs_val').replace('{u}',nvs.used).replace('{t}',nvs.total),nvs.free<150?'bad':nvs.free<250?'warn':'',nvsPct]:null,[i18n('sys_nvs_wr'),String(m.nvsWr||0)]]));
var pct=rssiToPct(w.rssi|0);
g.appendChild(sysCard(i18n('sys_net'),[[i18n('sys_ssid'),w.ssid||'—'],[i18n('sys_signal'),(w.rssi|0)+' dBm · '+pct+'%',pct<30?'bad':pct<55?'warn':'good',pct],[i18n('sys_ip'),w.ip||'—'],[i18n('sys_mac'),w.mac||'—'],[i18n('sys_mqtt'),m.mqtt?i18n('sys_on'):i18n('sys_off'),m.mqtt?'good':''],[i18n('sys_rescue'),w.rescue?i18n('sys_rescue_on'):i18n('sys_rescue_off'),w.rescue?'warn':'']]));
g.appendChild(sysCard(i18n('sys_sensors'),[['BME280',sn.bme?i18n('sys_ok'):i18n('sys_fail'),sn.bme?'good':'bad'],['DHT22',sn.dht?i18n('sys_ok'):i18n('sys_fail'),sn.dht?'good':'bad'],['BH1750',sn.lux?i18n('sys_ok'):i18n('sys_fail'),sn.lux?'good':'bad'],['AJ-SR04M',sn.water?i18n('sys_ok'):i18n('sys_fail'),sn.water?'good':'bad']]));
g.appendChild(sysCard(i18n('sys_stack'),[stk(i18n('sys_stack_core1'),st.core1),stk(i18n('sys_stack_loop'),st.loop),stk(i18n('sys_stack_tcp'),st.tcp),stk(i18n('sys_stack_wifi'),st.wifi)]));}
window.renderSysInfo=renderSysInfo;
// v6.3: журнал событий. Коды — как в event_log.h (EvType), не перенумеровывать.
var _evTimer=null,_evData=null,_evFilter='all';
// Время контроллера: tzOff (секунды от UTC) приходит в снимке состояния. Пока его нет —
// пояс браузера, как раньше. Расписание полива и часы в шапке живут по поясу
// контроллера, и журнал с графиком должны с ними сходиться.
var _tzOff=null,_histRaw=null;
function ctrlParts(ts){var d;if(_tzOff===null){d=new Date(ts*1000);return {y:d.getFullYear(),mo:d.getMonth(),d:d.getDate(),h:d.getHours(),mi:d.getMinutes()};}
d=new Date((ts+_tzOff)*1000);return {y:d.getUTCFullYear(),mo:d.getUTCMonth(),d:d.getUTCDate(),h:d.getUTCHours(),mi:d.getUTCMinutes()};}
function ctrlDayNo(ts){var p=ctrlParts(ts);return Math.floor(Date.UTC(p.y,p.mo,p.d)/86400000);}
function evPoll(on){clearInterval(_evTimer);_evTimer=null;if(!on)return;var ask=function(){if(ws&&ws.readyState===WebSocket.OPEN&&_wsAuthed)ws.send(JSON.stringify({cmd:'get_events'}));};ask();_evTimer=setInterval(ask,10000);}
var EV_RESET=['UNKNOWN','POWERON','EXT','SW','PANIC','INT_WDT','TASK_WDT','WDT','DEEPSLEEP','BROWNOUT','UNKNOWN'];
var EV_RELAY={0:'evn_irr1',1:'evn_irr2',2:'evn_irr3',7:'evn_humid',8:'evn_pump'};
var EV_SENS=['BME280','DHT22','BH1750','AJ-SR04M'];
// [текст, категория (water|vents|sys), важность (''|warn|bad|good)]
function evDescribe(t,a,v){var r=function(k,o){var x=i18n(k);for(var p in o)x=x.split('{'+p+'}').join(o[p]);return x;};
switch(t){
case 1:{var code=EV_RESET[a]||'UNKNOWN';return [r('ev_boot',{r:i18n('rr_'+code)}),'sys',['PANIC','INT_WDT','TASK_WDT','WDT','BROWNOUT'].indexOf(code)>=0?'bad':''];}
case 2:return [r(v?'ev_on':'ev_off',{n:i18n(EV_RELAY[a]||'evn_pump')}),'water',''];
case 3:return [r('ev_vent',{n:i18n(a?'ev_lower':'ev_upper'),a:(v>>8)&255,b:v&255}),'vents',''];
case 4:return [r(v?'ev_mode_auto':'ev_mode_manual',{n:i18n('evm_'+a)}),a===0?'vents':'water',''];
case 5:return [i18n(v?'ev_pump_fault':'ev_pump_ok'),'water',v?'bad':'good'];
case 6:return [r(v?'ev_sens_ok':'ev_sens_fail',{n:EV_SENS[a]||'?'}),'sys',v?'good':'bad'];
case 7:return [i18n(v?'ev_rain_on':'ev_rain_off'),'vents',v?'warn':''];
case 8:return [i18n(v?'ev_wifi_on':'ev_wifi_off'),'sys',v?'good':'warn'];
case 9:return [r('ev_auth',{n:v}),'sys','bad'];
case 10:return [a===9?i18n('ev_hist_cleared'):r('ev_set',{n:i18n('evs_'+a)}),'sys',a===8?'warn':''];
case 11:return [i18n(v?'ev_ota_ok':'ev_ota_fail'),'sys',v?'good':'bad'];
case 12:return [i18n(a?'ev_reboot_plan':'ev_reboot_panel'),'sys',''];
default:return ['#'+t,'sys',''];}}
function setEvFilter(f,btn){_evFilter=f;document.querySelectorAll('.ev-filter button').forEach(function(b){var on=b===btn;b.classList.toggle('active',on);b.setAttribute('aria-pressed',on?'true':'false');});if(_evData)renderEvents(_evData);}
window.setEvFilter=setEvFilter;
// Период показа: 1, 7 или 30 суток. Записи не удаляются — фильтр только на экране.
// «Сейчас» — по часам контроллера (m.now), иначе по браузеру. Записи без времени
// (до синхронизации часов, из этого запуска) показываются всегда.
var _evPeriod=30;try{var _p=+localStorage.getItem('gh_evp');if(_p===1||_p===7||_p===30)_evPeriod=_p;}catch(e){}
function setEvPeriod(days,btn){_evPeriod=days;try{localStorage.setItem('gh_evp',String(days));}catch(e){}
document.querySelectorAll('#evPeriod button').forEach(function(b){var on=b===btn;b.classList.toggle('active',on);b.setAttribute('aria-pressed',on?'true':'false');});
if(_evData)renderEvents(_evData);}
window.setEvPeriod=setEvPeriod;
function syncEvPeriodButtons(){document.querySelectorAll('#evPeriod button').forEach(function(b){var on=+b.dataset.days===_evPeriod;b.classList.toggle('active',on);b.setAttribute('aria-pressed',on?'true':'false');});}
function renderEvents(m){_evData=m;var box=$('evList');if(!box)return;box.textContent='';
var evFrom=((m.now|0)||Math.floor(Date.now()/1000))-_evPeriod*86400;
var list=(m.ev||[]).slice().reverse().map(function(e){var d=evDescribe(e[1],e[2],e[3]),ts=e[0],noTime=ts>=2147483648;return {ts:noTime?0:ts,up:noTime?ts-2147483648:0,text:d[0],cat:d[1],sev:d[2]};})
.filter(function(x){return (_evFilter==='all'||(_evFilter==='alarms'?(x.sev==='bad'||x.sev==='warn'):x.cat===_evFilter))&&(!x.ts||x.ts>=evFrom);});
if(!list.length){var em=document.createElement('div');em.className='ev-empty';em.textContent=i18n('ev_empty');box.appendChild(em);return;}
var todayNo=ctrlDayNo(Math.floor(Date.now()/1000));var lastGroup=null;
list.forEach(function(x){var g,tm;if(x.ts){var p=ctrlParts(x.ts);var diff=todayNo-ctrlDayNo(x.ts);
g=diff===0?i18n('ev_today'):diff===1?i18n('ev_yesterday'):pad2(p.d)+'.'+pad2(p.mo+1)+'.'+p.y;tm=pad2(p.h)+':'+pad2(p.mi);}
else{g=i18n('ev_notime');tm='+'+fmtUptime(x.up);}
if(g!==lastGroup){lastGroup=g;var h=document.createElement('div');h.className='ev-day';h.textContent=g;box.appendChild(h);}
var row=document.createElement('div');row.className='ev-row'+(x.sev?' '+x.sev:'');var t=document.createElement('span');t.className='ev-time';t.textContent=tm;var dot=document.createElement('span');dot.className='ev-dot ev-'+x.cat;var tx=document.createElement('span');tx.className='ev-text';tx.textContent=x.text;row.appendChild(t);row.appendChild(dot);row.appendChild(tx);box.appendChild(row);});}
window.renderEvents=renderEvents;
// v6.3: анонимная статистика (тумблер выключен по умолчанию; точный текст сообщения показывается здесь).
async function loadTelemetry(){var box=$('tlmBox');if(!box)return;try{var d=await fetchJson('/api/telemetry');
var cb=$('tlmEnable');cb.checked=!!d.enabled;cb.disabled=!d.available;$('tlmState').textContent=d.available?(d.enabled?i18n('tlm_on'):i18n('tlm_off')):'';
$('tlmNa').style.display=d.available?'none':'block';$('tlmId').textContent=d.id||'';$('tlmUrl').textContent=d.url||'';$('tlmUrlRow').style.display=d.available?'':'none';
$('tlmPayload').textContent=d.payload||'';
var st=$('tlmStatus');if(!d.available||!d.enabled){st.textContent='';}else if(d.lastOk){st.textContent=i18n('tlm_last')+': '+new Date(d.lastOk*1000).toLocaleString();}else if(d.lastCode&&d.lastCode!==0){st.textContent=i18n('tlm_err')+' '+d.lastCode;}else{st.textContent=i18n('tlm_last')+': '+i18n('tlm_never');}
}catch(e){}}
async function setTelemetry(on){try{await fetchJson('/api/telemetry',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:!!on})});toast(i18n('toast_saved'),'success');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}loadTelemetry();}
window.setTelemetry=setTelemetry;
function renderState(s){if(s.fwVersion)setFwVersion(s.fwVersion);
if(typeof s.tzOff==='number'&&s.tzOff!==_tzOff){_tzOff=s.tzOff;if(_histRaw)renderHistory(_histRaw);if(_evData)renderEvents(_evData);}
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
var sn=s.sensors||{};setGauge(GAUGES[0],sn.temp);setGauge(GAUGES[1],sn.humidity);setGauge(GAUGES[2],sn.pressure);
if(sn.lux!==undefined)$('luxVal').textContent=sn.lux;else $('luxVal').textContent='---';
// v6.1: блок DHT22 показывает НАРУЖНЫЕ значения (tempOut/humidityOut).
// Раньше сюда подставлялись sn.temp/sn.humidity — объединённые значения
// с приоритетом BME280, из-за чего блок дублировал стрелочные приборы.
$('dhtT').textContent=(typeof sn.tempOut==='number')?sn.tempOut.toFixed(1):'--.-';
$('dhtH').textContent=(typeof sn.humidityOut==='number')?sn.humidityOut.toFixed(1):'--.-';
window._isRaining=!!sn.rain;updateLuxGif(sn.lux||0,sn.lux!==undefined,window._isRaining);if(sn.bmeOk!==undefined){var chips=[{lbl:'BME280',ok:!!sn.bmeOk},{lbl:'DHT22',ok:sn.dhtOk},{lbl:'BH1750',ok:sn.luxOk},{lbl:'AJ-SR04M',ok:sn.waterOk}];$('sensorStatusRow').innerHTML=chips.map(function(c){return'<span class="sensor-chip '+(c.ok?'ok':'err')+'">'+(c.ok?'\u2713':'\u26a0')+' '+c.lbl+'</span>';}).join('');}
if(typeof sn.waterLevel==='number'){var p=Math.max(0,Math.min(100,sn.waterLevel));$('waterBarFill').style.height=p.toFixed(1)+'%';$('waterBarLabel').textContent=Math.round(p)+'%';}else{$('waterBarFill').style.height='0%';$('waterBarLabel').textContent='--%';}
if(s.relays){s.relays.forEach(function(state,idx){var btn=$('rBtn'+idx);if(btn){btn.classList.toggle('on',state);btn.classList.toggle('off',!state);btn.textContent=state?i18n('btn_on'):i18n('btn_off');}
if(idx===7){var hb=$('humidBadge');if(hb){hb.textContent=state?i18n('lbl_humid_on'):i18n('lbl_humid_off');hb.className='vent-badge '+(state?'open':'closed');}}
if(idx===8){var pb=$('pumpBadge');if(pb){pb.textContent=state?i18n('lbl_pump_on'):i18n('lbl_pump_off');pb.className='vent-badge '+(state?'open':'closed');}}});}
if(s.modes){applyModeVents(s.modes.vents===1);applyModeTimers(s.modes.timers===1);applyModeHumid(s.modes.humid===1);applyModePump(s.modes.pump===1);}
if(s.vents){updateVent(0,s.vents.upper);updateVent(1,s.vents.lower);var hom=!!((s.vents.upper&&s.vents.upper.calibrating)||(s.vents.lower&&s.vents.lower.calibrating));var hs=$('homingStatus');if(hs)hs.style.display=hom?'block':'none';var hd=$('homingDash');if(hd)hd.style.display=hom?'flex':'none';['btnHomeTop','btnHomeBottom'].forEach(function(id){var b=$(id);if(b)b.disabled=hom;});var stEl=$('ventAutoState');if(stEl&&s.vents.autoState!==undefined){var stKeys=['vstate_idle','vstate_open_top','vstate_open_bottom','vstate_close_top','vstate_close_bottom','vstate_pause','vstate_rain'];var k=stKeys[s.vents.autoState];stEl.textContent=k?i18n(k):'?';}var vrb=$('ventRainBadge');if(vrb)vrb.style.display=s.vents.rainBlocked?'inline-block':'none';}
if(s.timers){s.timers.forEach(function(t,i){onTimerState(i,t.active,t.remaining,t.nextStart);var fS=$('start'+i),fD=$('dur'+i),fI=$('int'+i);if(t.startMin!==undefined&&document.activeElement!==fS){var hh=Math.floor(t.startMin/60),mm=t.startMin%60;setVal('start'+i,pad2(hh)+':'+pad2(mm));}if(t.durationMin!==undefined&&document.activeElement!==fD)setVal('dur'+i,t.durationMin);if(t.intervalHours!==undefined&&document.activeElement!==fI)setVal('int'+i,t.intervalHours);});}
if(s.pump){$('pumpFaultWarn').style.display=s.pump.fault?'block':'none';}updateAlerts(s);}
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
function fmtCdParts(totalSec){totalSec=Math.max(0,totalSec|0);var totalMin=Math.ceil(totalSec/60);var d=Math.floor(totalMin/(60*24));var h=Math.floor((totalMin%(60*24))/60);var m=totalMin%60;return {d:pad2(d),h:pad2(h),m:pad2(m)};}function onTimerState(n,active,remaining,nextStart){var el=$('cd'+n);if(!el)return;if(active){el.innerHTML='<span class="cd-active">'+i18n('timer_active')+'</span>';el.className='countdown active';}else if(nextStart>=0){var f=fmtCdParts(nextStart);el.innerHTML='<span>'+f.d+'</span> <span>'+f.h+'</span><span class="seg-colon">:</span><span>'+f.m+'</span><small class="cd-u">'+i18n('cd_units')+'</small>';el.className='countdown';}else{el.innerHTML='<span>--</span> <span>--</span><span class="seg-colon">:</span><span>--</span><small class="cd-u">'+i18n('cd_units')+'</small>';el.className='countdown';}}// v6.1: подсвечиваем ту подпись тумблера, которая соответствует
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
function applyModeVents(a){markToggle('modeVents',a);var cb=$('modeVents');if(cb)cb.checked=a;var vs=$('ventAutoSum'),vsr=$('ventAutoStateRow'),vmb=$('ventManualBtns');if(vs)vs.style.display=a?'flex':'none';if(vsr)vsr.style.display=a?'block':'none';if(vmb)vmb.style.display=a?'none':'flex';}
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
function applyVentAuto(){var op=parseFloat($('vTOpen').value),cl=parseFloat($('vTClose').value);var tu=parseInt($('vuTravel').value,10),tl=parseInt($('vlTravel').value,10);if(isNaN(op)||isNaN(cl)){toast(i18n('err_fill_all'),'warning');return;}if(cl>=op){toast(i18n('err_close_lt_open'),'warning');return;}if(isNaN(tu)||tu<1||tu>120||isNaN(tl)||tl<1||tl>120){toast(i18n('err_calib_range'),'warning');return;}var nSent=0;if(op!==_lastFormVent.vTOpen||cl!==_lastFormVent.vTClose){if(savingSend({cmd:'set_thresholds',ventUpperHigh:op,ventUpperLow:cl,ventLowerHigh:op,ventLowerLow:cl})){_lastFormVent.vTOpen=op;_lastFormVent.vTClose=cl;}nSent++;}if(tu!==_lastFormVent.vuTravel){if(savingSend({cmd:'vent_set_calib',side:'top',sec:tu}))_lastFormVent.vuTravel=tu;nSent++;}if(tl!==_lastFormVent.vlTravel){if(savingSend({cmd:'vent_set_calib',side:'bottom',sec:tl}))_lastFormVent.vlTravel=tl;nSent++;}if(nSent===0)toast(i18n('msg_no_changes'),'info');}window.applyVentAuto=applyVentAuto;
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
function pumpResetFault(){if(wsSend({cmd:'force_pump_reset'}))toast(i18n('toast_fault_reset'),'success');}
window.pumpResetFault=pumpResetFault;function applyVentThresholds(){var op=parseFloat($('vTOpen').value),cl=parseFloat($('vTClose').value);if(isNaN(op)||isNaN(cl)){toast(i18n('err_fill_all'),'warning');return;}if(cl>=op){toast(i18n('err_close_lt_open'),'warning');return;}savingSend({cmd:'set_thresholds',ventUpperHigh:op,ventUpperLow:cl,ventLowerHigh:op,ventLowerLow:cl});}
window.applyVentThresholds=applyVentThresholds;function applyHumidThresholds(){var hmH=parseFloat($('hmH').value),hmL=parseFloat($('hmL').value);if(isNaN(hmH)||isNaN(hmL)||hmL>=hmH){toast(i18n('err_bad_thresholds'),'warning');return;}
savingSend({cmd:'set_thresholds',humidHigh:hmH,humidLow:hmL});}
window.applyHumidThresholds=applyHumidThresholds;function applyWaterCfg(){var lp=parseInt($('wtLP').value,10),hp=parseInt($('wtHP').value,10);var dp=parseInt($('wtDp').value,10),tm=parseInt($('wtTm').value,10);if([lp,hp,dp,tm].some(isNaN)){toast(i18n('err_fill_all'),'warning');return;}
if(lp>=hp){toast(i18n('err_on_lt_off'),'warning');return;}
savingSend({cmd:'set_water_tank',lowPct:lp,highPct:hp,depthCm:dp,pumpTimeoutMin:tm});}
window.applyWaterCfg=applyWaterCfg;// Сначала проверяем все три таймера, потом шлём: пустое время старта раньше молча
// превращалось в 00:00, а таймер с пустым полем пропускался без сообщения.
function applyAllTimers(){var cmds=[];for(var n=0;n<3;n++){var tm=/^(\d{1,2}):(\d{2})(?::\d{2})?$/.exec(($('start'+n).value||'').trim());var dur=parseInt($('dur'+n).value,10);var iv=parseInt($('int'+n).value,10);
if(!tm||+tm[1]>23||+tm[2]>59||isNaN(dur)||isNaN(iv)){toast(i18n('err_fill_all'),'warning');return;}
cmds.push({cmd:'set_timer',idx:n,startMin:(+tm[1])*60+(+tm[2]),durationMin:dur,intervalHours:iv});}
cmds.forEach(function(c){savingSend(c);});}
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
var held=false,beat=null;
// v6.3: пока кнопка держится, шлём биение каждые 500 мс. Контроллер
// останавливает привод, если биения нет дольше 2 с (обрыв связи, убитая вкладка).
function stopBeat(){if(beat){clearInterval(beat);beat=null;}}
function start(){if(held)return;held=true;if(!wsSend({cmd:'vent_hold',cmd_type:type})){held=false;return;}
stopBeat();beat=setInterval(function(){if(ws&&ws.readyState===WebSocket.OPEN&&_wsAuthed)ws.send('{"cmd":"vent_keepalive"}');},500);}
function stop(){stopBeat();if(!held)return;held=false;wsSend({cmd:'vent_hold',cmd_type:'stop'});}
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
function updateGaugeDom(g,value){var el=g.valId?$(g.valId):null;if(!el)return;var unitLbl=g.id==='cPress'?(currentLang==='ru'?'\u043c\u043c':'mm'):(g.id==='cTemp'?'\u00b0C':'%');var none=(isNaN(value)||value===null);el.textContent='';var num=document.createElement('span');num.className='gv-num';num.textContent=none?'--':g.fmt(value);var u=document.createElement('span');u.className='gv-u';u.textContent=unitLbl;el.appendChild(num);el.appendChild(u);el.style.color=none?'var(--text-muted)':zoneColor(value,g.zones);}
function drawGauge(g){var cv=$(g.id);if(!cv)return;var ctx=cv.getContext('2d'),dpr=window.devicePixelRatio||1;var W=cv.getBoundingClientRect().width||175,H=cv.getBoundingClientRect().height||99;if(cv.width!==Math.round(W*dpr)||cv.height!==Math.round(H*dpr)){cv.width=Math.round(W*dpr);cv.height=Math.round(H*dpr);}
var k=W/120;ctx.setTransform(dpr*k,0,0,dpr*k,0,0);ctx.clearRect(0,0,120,H/k);
var st=getComputedStyle(document.documentElement);var cTrack=st.getPropertyValue('--card-border').trim()||'#d4e0d4',cText=st.getPropertyValue('--text').trim()||'#1a2e1a',cDim=st.getPropertyValue('--text-dim').trim()||'#4a6a4a',cCard=st.getPropertyValue('--card').trim()||'#fff';
var cx=60,cy=60,r=44,span=g.max-g.min;
function arc(rr,f0,f1,w,col,alpha){ctx.save();ctx.globalAlpha=alpha||1;ctx.lineWidth=w;ctx.strokeStyle=col;ctx.lineCap='butt';ctx.beginPath();ctx.arc(cx,cy,rr,Math.PI*(f0-1),Math.PI*(f1-1));ctx.stroke();ctx.restore();}
var from=0;g.zones.forEach(function(z){var to=Math.max(0,Math.min(1,(z.to-g.min)/span));if(to>from)arc(r+9,from,to,3,z.c,.55);from=Math.max(from,to);});
arc(r,0,1,10,cTrack);
var has=!isNaN(g.value),f=has?Math.max(0,Math.min(1,(g.value-g.min)/span)):0;
if(has&&f>0.002)arc(r,0,f,10,zoneColor(g.value,g.zones));
for(var i=0;i<=10;i++){var ta=Math.PI*(1-i/10),big=i%5===0,r1=r-7,r2=r-(big?13:10);ctx.save();ctx.globalAlpha=big?.8:.45;ctx.strokeStyle=cDim;ctx.lineWidth=big?1.4:1;ctx.beginPath();ctx.moveTo(cx+r1*Math.cos(ta),cy-r1*Math.sin(ta));ctx.lineTo(cx+r2*Math.cos(ta),cy-r2*Math.sin(ta));ctx.stroke();ctx.restore();}
if(has){var na=Math.PI*(1-f);ctx.save();ctx.strokeStyle=cText;ctx.lineWidth=2.2;ctx.lineCap='round';ctx.beginPath();ctx.moveTo(cx,cy);ctx.lineTo(cx+(r-4)*Math.cos(na),cy-(r-4)*Math.sin(na));ctx.stroke();ctx.restore();}
ctx.fillStyle=cText;ctx.beginPath();ctx.arc(cx,cy,5,0,Math.PI*2);ctx.fill();ctx.fillStyle=cCard;ctx.beginPath();ctx.arc(cx,cy,2.2,0,Math.PI*2);ctx.fill();}
function animateTo(g,target,dur){var from=isNaN(g.value)?g.min:g.value,t0=performance.now();(function step(now){var tk=Math.min(1,(now-t0)/dur);var e=tk<0.5?2*tk*tk:-1+(4-2*tk)*tk;g.value=from+(target-from)*e;drawGauge(g);if(tk<1)requestAnimationFrame(step);})(t0);}
function redrawAll(){GAUGES.forEach(function(g){drawGauge(g);});}
// Нет числа в снимке (датчик отказал) — прибор показывает «--» без стрелки. Раньше он
// застывал на последнем значении и выдавал его за текущее.
function setGauge(g,v){if(typeof v==='number'){animateTo(g,v,700);updateGaugeDom(g,v);}else if(!isNaN(g.value)){g.value=NaN;updateGaugeDom(g,NaN);drawGauge(g);}}
// v6.3: контроллер отдаёт неделю истории столбцами (t:[...], T:[...], ...; null — нет данных).
function histColumns(m){var t=m.t||[],keys=['T','H','P','L','W','T2','H2'],out=[];for(var i=0;i<t.length;i++){var p={t:t[i]};keys.forEach(function(k){if(m[k]&&m[k][i]!=null)p[k]=m[k][i];});out.push(p);}return out;}
// Период графика: 24 — сутки, 168 — неделя. Точки храним все, показываем срез.
var _histAll=[],_histRange=24;
function applyHistRange(){if(!_histAll.length){histData=[];return;}var last=_histAll[_histAll.length-1].ts,from=last-(_histRange-1)*3600-600;histData=_histAll.filter(function(p){return p.ts>=from;});}
function setHistRange(h,btn){_histRange=h;btn.parentNode.querySelectorAll('button').forEach(function(b){b.classList.toggle('active',b===btn);b.setAttribute('aria-pressed',b===btn?'true':'false');});_chartSel=null;applyHistRange();drawHistChart(activeChart);updateHistStats(activeChart);}
window.setHistRange=setHistRange;
function renderHistory(points){_histRaw=points;if(!Array.isArray(points)||!points.length){_histAll=[];histData=[];drawHistChart(activeChart);updateHistStats(activeChart);return;}
_histAll=points.map(function(p){var q=ctrlParts(p.t);return{ts:p.t,h:q.h,dd:pad2(q.d)+'.'+pad2(q.mo+1),t:p.T,hu:p.H,p:p.P,w:p.W,l:p.L,t2:p.T2,hu2:p.H2};});applyHistRange();drawHistChart(activeChart);updateHistStats(activeChart);}
function showChart(type,btn){activeChart=type;_chartSel=null;document.querySelectorAll('.chart-tab').forEach(function(b){b.classList.remove('active');});if(btn)btn.classList.add('active');drawHistChart(type);updateHistStats(type);}
window.showChart=showChart;// v6.3: мин / среднее / макс / последнее для рядов активной вкладки за выбранный период.
function fmtNum(v){return (Math.round(v*10)/10).toFixed(1);}
function updateHistStats(type){var box=$('histStats');if(!box)return;box.textContent='';
var map={temp:[['t',i18n('chart_inside'),'\u00b0C'],['t2',i18n('chart_outside'),'\u00b0C']],hum:[['hu',i18n('chart_inside'),'%'],['hu2',i18n('chart_outside'),'%']],press:[['p',i18n('tab_press'),currentLang==='ru'?'\u043c\u043c':'mm']],water:[['w',i18n('tab_water'),'%']]};
var rows=(map[type]||[]).map(function(d){var v=(histData||[]).map(function(p){return p[d[0]];}).filter(function(x){return x!=null&&!isNaN(x);});return {d:d,v:v};}).filter(function(r){return r.v.length;});
if(!rows.length){var e=document.createElement('div');e.className='hint';e.textContent=i18n('st_none');box.appendChild(e);return;}
var tb=document.createElement('table');tb.className='hist-stats-table';var hr=document.createElement('tr');[i18n('st_series'),i18n('st_min'),i18n('st_avg'),i18n('st_max'),i18n('st_last')].forEach(function(t){var th=document.createElement('th');th.textContent=t;hr.appendChild(th);});tb.appendChild(hr);
rows.forEach(function(r){var v=r.v,sum=v.reduce(function(a,b){return a+b;},0),tr=document.createElement('tr');[r.d[1],fmtNum(Math.min.apply(null,v))+' '+r.d[2],fmtNum(sum/v.length)+' '+r.d[2],fmtNum(Math.max.apply(null,v))+' '+r.d[2],fmtNum(v[v.length-1])+' '+r.d[2]].forEach(function(t,i){var td=document.createElement(i?'td':'th');td.textContent=t;tr.appendChild(td);});tb.appendChild(tr);});box.appendChild(tb);}
function drawHistChart(type){
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
if(n<=48)pts.forEach(function(pt){ctx.beginPath();ctx.arc(pt.x,pt.y,3.2,0,Math.PI*2);ctx.fill();ctx.stroke();});
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
if(_histRange>24){for(var di=0;di<n;di++)if(histData[di].h===0)ctx.fillText(histData[di].dd,xOf(di),H-pad.b+14);}else{for(var li=0;li<n;li+=step)ctx.fillText(labels[li],xOf(li),H-pad.b+14);}
ctx.restore();
_chartGeom={n:n,padL:pad.l,cw:cw};
if(_chartSel!==null&&_chartSel>=0&&_chartSel<n){
var sx=xOf(_chartSel),pp=histData[_chartSel],dt=ctrlParts(pp.ts);
ctx.save();ctx.strokeStyle=textCol;ctx.lineWidth=1;ctx.setLineDash([3,3]);ctx.beginPath();ctx.moveTo(sx,pad.t);ctx.lineTo(sx,H-pad.b);ctx.stroke();ctx.setLineDash([]);
var tl=[pad2(dt.d)+'.'+pad2(dt.mo+1)+' '+pad2(dt.h)+':'+pad2(dt.mi)];
series.forEach(function(s){var v=pp[s.key];if(v==null||isNaN(v))return;ctx.beginPath();ctx.arc(sx,yOf(v),5,0,Math.PI*2);ctx.fillStyle=s.color;ctx.fill();tl.push((series.length>1?s.label+': ':'')+v.toFixed(1)+' '+UNIT);});
ctx.font='12px Arial';ctx.textAlign='left';ctx.textBaseline='middle';
var tw=0;tl.forEach(function(t){tw=Math.max(tw,ctx.measureText(t).width);});tw+=16;var th=tl.length*17+8;
var bx=sx+10;if(bx+tw>W-2)bx=sx-10-tw;if(bx<2)bx=2;var by=pad.t+2;
ctx.fillStyle=st.getPropertyValue('--card').trim()||'#fff';ctx.strokeStyle=gridCol;ctx.lineWidth=1;ctx.fillRect(bx,by,tw,th);ctx.strokeRect(bx,by,tw,th);
ctx.fillStyle=st.getPropertyValue('--text').trim()||'#1a2e1a';tl.forEach(function(t,i){ctx.fillText(t,bx+8,by+4+8.5+i*17);});
ctx.restore();}
}
// v6.3: касание или наведение на график показывает значения выбранной точки.
var _chartGeom=null,_chartSel=null;
function chartPick(ev){var cv=$('histChart');if(!cv||!_chartGeom||_chartGeom.n<2)return;var r=cv.getBoundingClientRect();var i=Math.round((ev.clientX-r.left-_chartGeom.padL)/_chartGeom.cw*(_chartGeom.n-1));i=Math.max(0,Math.min(_chartGeom.n-1,i));if(i!==_chartSel){_chartSel=i;drawHistChart(activeChart);}}
function chartClear(){if(_chartSel!==null){_chartSel=null;drawHistChart(activeChart);}}
function setupChartTouch(){var cv=$('histChart');if(!cv)return;cv.style.touchAction='pan-y';cv.addEventListener('pointerdown',chartPick);cv.addEventListener('pointermove',function(ev){if(ev.pointerType==='mouse'||ev.buttons)chartPick(ev);});cv.addEventListener('pointerleave',function(ev){if(ev.pointerType==='mouse')chartClear();});}
// v6.1: контроллер отвечает на 401 без заголовка WWW-Authenticate, поэтому
// системное окно браузера больше не всплывает. Обрабатываем отказ сами:
// сбрасываем сессию и возвращаем пользователя на форму входа панели.
function onAuthLost(){
  _authUser='';_authToken='';
  try{_ss.removeItem('gh_u');_ss.removeItem('gh_t');}catch(e){}
  stopStatePoll();
  if(ws&&ws.readyState===WebSocket.OPEN)ws.close();
  showLoginScreen();
  var err=$('loginError');
  if(err){err.textContent=i18n('ws_auth_fail');err.style.display='block';}
}
async function fetchJson(url,options){var opts=options||{};if(!opts.headers||!opts.headers['Authorization']){opts.headers=authHeaders(opts.headers||{});}
var resp=await fetch(url,opts);
if(resp.status===401){onAuthLost();throw new Error('HTTP 401');}
// 403 default_credentials — контроллер ждёт смены заводского пароля.
if(resp.status===403){var j=null;try{j=await resp.json();}catch(e){}if(j&&j.error==='default_credentials'){_credsDefault=true;showAppShell();throw new Error(i18n('err_default_creds'));}
if(j&&j.error==='current password is wrong')throw new Error(i18n('err_cur_password_wrong'));}
if(!resp.ok)throw new Error('HTTP '+resp.status);return await resp.json();}
// Контроллер недоступен — падают все три запроса. Формы сети и MQTT всё равно
// пробуем заполнить (пустая форма сети при «Сохранить» стёрла бы SSID), но
// тост об ошибке показываем один, а не три подряд.
var _loadErrShown=false;
async function loadAllSettings(){_loadErrShown=false;try{var s=await fetchJson('/api/settings');fillSettingsForm(s);}catch(e){_loadErrShown=true;toast(i18n('err_load_settings')+': '+e.message,'error');}
await loadNetSettings();await loadMqttSettings();_loadErrShown=false;}
function safeSet(id,v){var el=$(id);if(!el||v===undefined||v===null)return;if(document.activeElement===el||el.dataset.dirty)return;el.value=v;}// v6.3: пороги автоматики живут во вкладке «Автоматика», но нужны и главному экрану:
// сводка в карточках и зоны цвета на шкалах (раньше 18/28 °C и 50/79 % были зашиты).
var _cfg={vOpen:NaN,vClose:NaN,hOn:NaN,hOff:NaN,wOn:NaN,wOff:NaN};
function _cfgFrom(s){if(s.ventUpper){_cfg.vOpen=+s.ventUpper.high;_cfg.vClose=+s.ventUpper.low;}if(s.humid){_cfg.hOn=+s.humid.low;_cfg.hOff=+s.humid.high;}if(s.waterTank){_cfg.wOn=+s.waterTank.lowPct;_cfg.wOff=+s.waterTank.highPct;}syncCfgViews();}
function _sumText(key,a,b,ka,kb){return(isFinite(a)&&isFinite(b))?i18n(key).replace(ka,a).replace(kb,b):'--';}
function syncCfgViews(){
var gt=GAUGES[0];if(isFinite(_cfg.vClose)&&isFinite(_cfg.vOpen)&&_cfg.vClose<_cfg.vOpen)gt.zones=[{to:Math.min(18,_cfg.vClose),c:'#38bdf8'},{to:_cfg.vOpen,c:'#4ade80'},{to:gt.max,c:'#f87171'}];
var gh=GAUGES[1];if(isFinite(_cfg.hOn)&&isFinite(_cfg.hOff)&&_cfg.hOn<_cfg.hOff)gh.zones=[{to:_cfg.hOn,c:'#38bdf8'},{to:_cfg.hOff,c:'#4ade80'},{to:gh.max,c:'#f87171'}];
GAUGES.forEach(function(g){updateGaugeDom(g,g.value);drawGauge(g);});
var e=$('ventSumTxt');if(e)e.textContent=_sumText('sum_vents',_cfg.vOpen,_cfg.vClose,'{o}','{c}');
e=$('humidSumTxt');if(e)e.textContent=_sumText('sum_humid',_cfg.hOn,_cfg.hOff,'{l}','{h}');
e=$('pumpSumTxt');if(e)e.textContent=_sumText('sum_pump',_cfg.wOn,_cfg.wOff,'{l}','{h}');
// v6.3: метки порогов насоса на шкале ёмкости — видно, где он включится и где остановится.
[['wbMarkOn',_cfg.wOn,'lbl_pump_on_pct'],['wbMarkOff',_cfg.wOff,'lbl_pump_off_pct']].forEach(function(m){var el=$(m[0]);if(!el)return;var ok=isFinite(m[1]);el.style.display=ok?'block':'none';if(ok){el.style.bottom=Math.max(0,Math.min(100,m[1]))+'%';el.title=i18n(m[2])+' '+m[1];}});}
function goAuto(){showPage('settings');showSettingsTab('auto');}
window.goAuto=goAuto;
// v6.3: «Настроить» в карточке открывает настройки только своей подсистемы — окном поверх
// главного экрана. Поля не дублируются: секция из вкладки «Автоматика» на время показа
// переносится в окно и при закрытии возвращается на место, поэтому id, обработчики
// и заполнение формы (fillSettingsForm) остаются прежними.
var CFG_SECS={vents:['autoSecVents','lbl_vents'],humid:['autoSecHumid','lbl_humidifier'],pump:['autoSecPump','lbl_water_tank']};
var _cfgSheet=null;
function openCfgSheet(key){var d=CFG_SECS[key];if(!d)return;if(_cfgSheet)closeCfgSheet();var sec=$(d[0]),body=$('cfgSheetBody');if(!sec||!body)return;
_cfgSheet={sec:sec,parent:sec.parentNode,next:sec.nextSibling,opener:document.activeElement};
body.appendChild(sec);$('cfgSheetTitle').textContent=i18n(d[1]);$('cfgSheet').classList.add('open');$('cfgOverlay').classList.add('open');
// Фокус на «закрыть», а не на первое поле: на телефоне иначе сразу выскочит клавиатура.
var c=document.querySelector('#cfgSheet .cfg-close');if(c)c.focus();
loadAllSettings();}
function closeCfgSheet(){if(!_cfgSheet)return;var s=_cfgSheet;_cfgSheet=null;s.parent.insertBefore(s.sec,s.next);$('cfgSheet').classList.remove('open');$('cfgOverlay').classList.remove('open');if(s.opener&&s.opener.focus)s.opener.focus();}
window.openCfgSheet=openCfgSheet;window.closeCfgSheet=closeCfgSheet;
// v6.3: зона тревог над карточками — то, что требует внимания, видно сразу.
// DOM перестраивается только при смене набора тревог, иначе экранный диктор
// повторял бы их каждую секунду.
var _alertSig='';
function updateAlerts(s){var z=$('alertZone');if(!z)return;var items=[];var sn=s.sensors||{};
if(s.pump&&s.pump.fault)items.push({k:'pump',t:i18n('lbl_pump_fault'),cls:'err',act:true});
[['bmeOk','BME280'],['dhtOk','DHT22'],['luxOk','BH1750'],['waterOk','AJ-SR04M']].forEach(function(p){if(sn[p[0]]===false)items.push({k:p[0],t:i18n('alert_sensor').replace('{n}',p[1]),cls:'err'});});
if(s.vents&&s.vents.rainBlocked)items.push({k:'rain',t:i18n('alert_rain'),cls:'warn'});
var sig=currentLang+'|'+items.map(function(i){return i.k;}).join(',');if(sig===_alertSig)return;_alertSig=sig;
z.textContent='';items.forEach(function(it){var d=document.createElement('div');d.className='alert-item '+it.cls;var sp=document.createElement('span');sp.textContent='\u26a0 '+it.t;d.appendChild(sp);if(it.act){var b=document.createElement('button');b.type='button';b.className='btn-cal';b.textContent=i18n('btn_reset_fault');b.addEventListener('click',pumpResetFault);d.appendChild(b);}z.appendChild(d);});
z.style.display=items.length?'flex':'none';}
function fillSettingsForm(s){_cfgFrom(s);if(s.otaPassSet!==undefined){var ops=$('otaPassState');if(ops)ops.textContent=i18n(s.otaPassSet?'ota_pass_on':'ota_pass_off');}if(s.credsDefault!==undefined&&!!s.credsDefault!==_credsDefault){_credsDefault=!!s.credsDefault;showAppShell();}if(s.ventUpper){safeSet('vTOpen',s.ventUpper.high);safeSet('vTClose',s.ventUpper.low);_lastFormVent.vTOpen=s.ventUpper.high;_lastFormVent.vTClose=s.ventUpper.low;if(s.ventUpper.travelMs!==undefined){var tuv=Math.round(s.ventUpper.travelMs/1000);safeSet('vuTravel',tuv);_lastFormVent.vuTravel=tuv;}}if(s.ventLower){if(s.ventLower.travelMs!==undefined){var tlv=Math.round(s.ventLower.travelMs/1000);safeSet('vlTravel',tlv);_lastFormVent.vlTravel=tlv;}}if(s.humid){safeSet('hmL',s.humid.low);safeSet('hmH',s.humid.high);}if(s.waterTank){safeSet('wtLP',s.waterTank.lowPct);safeSet('wtHP',s.waterTank.highPct);safeSet('wtDp',s.waterTank.depthCm);safeSet('wtTm',s.waterTank.pumpTimeoutMin);}
if(s.timers){s.timers.forEach(function(t,i){var h=Math.floor(t.startMin/60),m=t.startMin%60;safeSet('start'+i,pad2(h)+':'+pad2(m));safeSet('dur'+i,t.durationMin);safeSet('int'+i,t.intervalHours);});}
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
var TZ_POSIX_MAP={'UTC+12':'UTC-12','UTC+11':'UTC-11','UTC+10':'UTC-10','UTC+9':'UTC-9','UTC+8':'UTC-8','UTC+7':'UTC-7','UTC+6':'UTC-6','UTC+5':'UTC-5','UTC+4':'UTC-4','UTC+3':'UTC-3','UTC+2':'UTC-2','UTC+1':'UTC-1','UTC0':'UTC+0','UTC-1':'UTC+1','UTC-2':'UTC+2','MSK-3':'MSK','UTC-4':'UTC+4','UTC-5':'UTC+5','UTC-6':'UTC+6','UTC-7':'UTC+7','UTC-8':'UTC+8','UTC-9':'UTC+9','UTC-10':'UTC+10','UTC-11':'UTC+11','UTC-12':'UTC+12'};// Пояс контроллера, которого нет в списке (правила летнего времени, 'UTC-3'), раньше
// оставлял выбранной первую опцию (UTC-12) — и «Сохранить NTP» затирал им настоящий
// пояс, сдвигая часы и расписание полива. Теперь такой пояс — отдельный пункт как есть.
function selectTzByPosix(posix){var sel=$('ntpTzSel');if(!sel)return;var val=TZ_POSIX_MAP[posix];var opt=null;
for(var i=0;i<sel.options.length;i++){if(val&&sel.options[i].value===val){opt=sel.options[i];break;}}
if(!opt){opt=sel.querySelector('option[data-custom]');if(!opt){opt=document.createElement('option');opt.setAttribute('data-custom','1');opt.value='custom';sel.appendChild(opt);}opt.setAttribute('data-posix',posix);opt.textContent=posix;}
sel.value=opt.value;}
function onTzSelChange(){var sel=$('ntpTzSel');if(!sel)return;var opt=sel.options[sel.selectedIndex];if(opt){var ntpStatus=$('ntpStatus');if(ntpStatus)ntpStatus.textContent='POSIX: '+opt.getAttribute('data-posix');}}
window.onTzSelChange=onTzSelChange;async function applyNtp(){var server=($('ntpServer').value||'pool.ntp.org').trim();var sel=$('ntpTzSel');if(!sel){toast(i18n('err_generic'),'error');return;}
var opt=sel.options[sel.selectedIndex];var posix=opt?opt.getAttribute('data-posix'):'UTC0';var hrs=parseInt(($('ntpUpdateSel').value||'4'),10);try{var r=await fetchJson('/api/ntp',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({server:server,tz:posix,updateHrs:hrs})});if(r.ok)toast(i18n('toast_saved'),'success');else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyNtp=applyNtp;async function loadNetSettings(){try{var n=await fetchJson('/api/network');setVal('ssid',n.ssid);setVal('wPas','');$('modeSwitch').checked=!n.isDHCP;onDhcpModeChange();setVal('ipAddr',n.ip);setVal('subnet',n.subnet);setVal('gateway',n.gateway);setVal('dns1',n.dns1);setVal('dns2',n.dns2);$('infoMac')&&($('infoMac').textContent=n.mac||'—');}catch(e){if(!_loadErrShown)toast(i18n('err_load_network'),'error');}}
window.loadNetSettings=loadNetSettings;async function loadMqttSettings(){try{var m=await fetchJson('/api/mqtt');$('mqttEnable').checked=!!m.enabled;markMqttEnable();setVal('mqttServer',m.server);setVal('mqttPort',m.port);setVal('mqttUser',m.user);setVal('mqttPass','');renderMqttState(m.connected);}catch(e){if(!_loadErrShown)toast(i18n('err_load_mqtt'),'error');}}
window.loadMqttSettings=loadMqttSettings;function onDhcpModeChange(){var isStatic=$('modeSwitch').checked;markToggle('modeSwitch',isStatic,'.dhcp-lbl');$('staticFields').style.display=isStatic?'block':'none';$('dhcpHint').style.display=isStatic?'none':'block';}
window.onDhcpModeChange=onDhcpModeChange;async function applyNet(){var body={ssid:$('ssid').value,pass:$('wPas').value,isDHCP:!$('modeSwitch').checked,ip:$('ipAddr').value,subnet:$('subnet').value,gateway:$('gateway').value,dns1:$('dns1').value,dns2:$('dns2').value};try{var r=await fetchJson('/api/network',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(r.ok)toast(i18n('toast_saved')+'. '+i18n('msg_rebooting'),'success');else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyNet=applyNet;async function resetWifi(){if(!confirm(i18n('toast_confirm_forget_wifi')))return;try{await fetchJson('/api/reset_wifi',{method:'POST'});toast(i18n('msg_rebooting'),'info');}
catch(e){toast(e.message,'error');}}
window.resetWifi=resetWifi;// Скан на контроллере асинхронный: пока он идёт, ответ {"scanning":true} — спрашиваем
// снова раз в секунду (до 20 раз), пока не придёт список.
function scanWifi(tries){tries=tries|0;var list=$('wifiScanList');if(!list)return;list.style.display='block';if(!tries)list.textContent=i18n('wifi_scanning');fetchJson('/api/wifi_scan').then(function(d){if(d.scanning){if(tries<20)setTimeout(function(){scanWifi(tries+1);},1000);else list.textContent=i18n('wifi_scan_failed');return;}if(!d.networks||!d.networks.length){list.textContent=i18n('wifi_none');return;}
// v6.3: имена сетей приходят из эфира, то есть от любого соседа, — вставлять
// их как разметку нельзя. Раньше шло через innerHTML и inline-onclick: имя
// вида <img onerror=...> выполнялось в панели, где в sessionStorage лежит
// пароль, а имя с апострофом (Ivan's) ломало выбор сети.
list.textContent='';
d.networks.forEach(function(n){var it=document.createElement('div');it.className='wifi-item';
it.textContent=n.ssid+' ('+n.rssi+' dBm'+(n.enc?' 🔒':'')+')';
it.addEventListener('click',function(){setVal('ssid',n.ssid);});
list.appendChild(it);});}).catch(function(){list.textContent=i18n('wifi_scan_failed');});}
window.scanWifi=scanWifi;async function applyMqtt(){var body={enabled:$('mqttEnable').checked,server:$('mqttServer').value,port:parseInt($('mqttPort').value,10),user:$('mqttUser').value,pass:$('mqttPass').value};try{var r=await fetchJson('/api/mqtt',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(r.ok)toast(i18n('toast_saved'),'success');else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyMqtt=applyMqtt;function mqttResendDiscovery(){if(wsSend({cmd:'mqtt_resend_discovery'}))toast(i18n('toast_discovery_sent'),'success');}
window.mqttResendDiscovery=mqttResendDiscovery;// Смена пароля сбрасывает на контроллере все сессии, включая эту; в ответе — новый токен.
// Опрос состояния на время запроса останавливаем, а сокет после ответа переоткрываем
// с новым токеном: иначе ближайший get_state по старой сессии получил бы отказ и разлогинил.
// У старого сокета снимаем и onmessage: сокет и POST — разные соединения, и кадр, ушедший
// до POST, контроллер мог разобрать уже после сброса сессий и ответить «session expired».
// Текущий пароль обязателен: украденный токен сессии сам по себе пароль сменить не даёт.
async function applyWebCreds(){var c=$('wbCurPass').value,u=$('wbUser').value.trim(),p=$('wbPass').value;if(!c){toast(i18n('err_cur_password'),'warning');return;}if(u.length<1||p.length<8){toast(i18n('err_creds_len'),'warning');return;}
stopStatePoll();
try{var r=await fetchJson('/api/webcreds',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({curPass:c,user:u,pass:p})});if(r.ok&&r.token){toast(i18n('toast_saved'),'success');_authUser=u;_authToken=r.token;_ss.setItem('gh_u',u);_ss.setItem('gh_t',r.token);$('wbPass').value='';$('wbCurPass').value='';_credsDefault=false;showAppShell();if(ws&&ws.readyState!==WebSocket.CLOSED){ws.onclose=null;ws.onmessage=null;ws.close();}ws=null;wsConnect();}else{toast(r.error||i18n('err_generic'),'error');startStatePoll();}}catch(e){toast(i18n('err_generic')+': '+e.message,'error');startStatePoll();}}
window.applyWebCreds=applyWebCreds;
async function applyOtaPass(){var el=$('otaPass'),p=el?el.value:'';if(p.length&&(p.length<8||p.length>31)){toast(i18n('err_ota_pass_len'),'warning');return;}
try{var r=await fetchJson('/api/otapass',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pass:p})});if(r.ok){toast(i18n('ota_pass_saved'),'success');el.value='';loadAllSettings();}else toast(r.error||i18n('err_generic'),'error');}catch(e){toast(i18n('err_generic')+': '+e.message,'error');}}
window.applyOtaPass=applyOtaPass;async function sysReboot(){if(!confirm(i18n('toast_confirm_reboot')))return;try{await fetchJson('/api/reboot',{method:'POST'});toast(i18n('msg_rebooting'),'info');}
catch(e){toast(e.message,'error');}}
window.sysReboot=sysReboot;
// v6.1: было fetchJson('/api/clear_history') — такого маршрута в прошивке
// НЕТ, кнопка возвращала 404 и историю не чистила. Команда clear_history
// в WebSocket при этом существовала и работала. Переведено на неё.
// Тост «Сохранено» — по ack (savingSend), а локальные точки чистятся в onMessage при
// подтверждении: раньше чистился только histData, и переключение «Сутки/Неделя»
// возвращало старый график из _histAll.
function sysClearHistory(){if(!confirm(i18n('toast_confirm_clear')))return;savingSend({cmd:'clear_history'});}
window.sysClearHistory=sysClearHistory;async function sysFactoryReset(){if(!confirm(i18n('toast_confirm_factory')))return;if(!confirm(i18n('toast_confirm_factory2')))return;try{await fetchJson('/api/factory_reset',{method:'POST'});toast(i18n('msg_resetting'),'warning');}
catch(e){toast(e.message,'error');}}
window.sysFactoryReset=sysFactoryReset;function otaValidateFile(){var fInput=$('otaFile'),f=fInput.files[0];var det=$('otaDetected'),btn=$('btnOtaUpload'),nameEl=$('otaFileName');if(!f){if(nameEl)nameEl.textContent=i18n('set_ota_no_file');det.textContent='';btn.disabled=true;return null;}if(nameEl)nameEl.textContent=f.name;var rx=/^greenhouse_v(\d+)\.(\d+)\.bin$/i;var m=rx.exec(f.name);if(m){det.textContent=i18n('ota_detected_container').replace('{v}','v'+m[1]+'.'+m[2]);det.style.color='var(--on-col)';btn.disabled=false;return 'container';}else{det.textContent=i18n('ota_bad_name');det.style.color='var(--off-col)';btn.disabled=true;return null;}}
window.otaValidateFile=otaValidateFile;// v6.3: после 100 % загрузки контроллер ещё проверяет контрольные суммы и пишет образ во флеш
// (до полуминуты), затем перезагружается. Всё это время — мигающее предупреждение поверх
// страницы и запрос подтверждения при попытке закрыть вкладку.
var _otaCountdown=null;
function otaBeforeUnload(e){e.preventDefault();e.returnValue='';return '';}
function showOtaWarn(text){var w=$('otaWarn');if(!w)return;$('otaWarnText').textContent=text;w.style.display='flex';window.addEventListener('beforeunload',otaBeforeUnload);}
function hideOtaWarn(){var w=$('otaWarn');if(w)w.style.display='none';clearInterval(_otaCountdown);_otaCountdown=null;window.removeEventListener('beforeunload',otaBeforeUnload);}
function otaRebootCountdown(sec){var left=sec;var tick=function(){$('otaWarnText').textContent=i18n('ota_rebooting').replace('{n}',left);if(left<=0){clearInterval(_otaCountdown);window.removeEventListener('beforeunload',otaBeforeUnload);location.reload();}left--;};showOtaWarn('');tick();_otaCountdown=setInterval(tick,1000);}
function otaUpload(){var type=otaValidateFile();if(!type){toast(i18n('ota_bad_name'),'error');return;}var file=$('otaFile').files[0];var fd=new FormData();fd.append('firmware',file,file.name);$('otaProgress').style.display='block';$('btnOtaUpload').disabled=true;var xhr=new XMLHttpRequest();xhr.open('POST','/update');xhr.setRequestHeader('Authorization','Bearer '+_authToken);xhr.upload.addEventListener('progress',function(e){if(e.lengthComputable){var pct=Math.round(e.loaded/e.total*100);$('otaBar').style.width=pct+'%';$('otaText').textContent=pct+'%';if(pct>=100)showOtaWarn(i18n('ota_wait'));}});xhr.addEventListener('load',function(){$('btnOtaUpload').disabled=false;if(xhr.status===200){toast(i18n('ota_done'),'success');otaRebootCountdown(12);}else{hideOtaWarn();var err=i18n('ota_failed')+' ('+xhr.status+')';try{var r=JSON.parse(xhr.responseText);if(r.error)err=r.error;}catch(e){}toast(err,'error');}});xhr.addEventListener('error',function(){hideOtaWarn();$('btnOtaUpload').disabled=false;toast(i18n('ota_failed'),'error');});xhr.send(fd);}
window.otaUpload=otaUpload;function openAbout(){$('aboutModal').classList.add('open');$('aboutOverlay').classList.add('open');}
function closeAbout(){$('aboutModal').classList.remove('open');$('aboutOverlay').classList.remove('open');}
window.openAbout=openAbout;window.closeAbout=closeAbout;document.addEventListener('DOMContentLoaded',function(){try{var l=localStorage.getItem(LANG_KEY);if(l&&GH_I18N[l])currentLang=l;else if(/^ru\b/i.test(navigator.language||''))currentLang='ru';}catch(e){}
try{var t=localStorage.getItem('gh_theme');if(t!=null)applyTheme(t);}catch(e){}
applyLang(currentLang);[$('loginUser'),$('loginPass')].forEach(function(el){if(el)el.addEventListener('keydown',loginKeydown);});if(_authToken){showAppShell();wsConnect();loadAllSettings();}else{showLoginScreen();}['vents','timers','humid','pump'].forEach(function(key){var el=$('mode'+key.charAt(0).toUpperCase()+key.slice(1));if(el)el.addEventListener('change',function(){onModeChange(key,el.checked);});});syncEvPeriodButtons();setupVentHoldButtons();setupChartTouch();setupTabsHint();setupThemeTiles();document.addEventListener('input',markDirty);document.addEventListener('visibilitychange',onVisibility);
(function(){var mq=$('mqttEnable');if(mq)mq.addEventListener('change',function(){markMqttEnable();renderMqttState();});})();var cs=$('cfgSheet');if(cs)cs.addEventListener('click',function(e){if(e.target===cs)closeCfgSheet();});var al=$('aboutLink');if(al)al.addEventListener('click',function(e){e.preventDefault();openAbout();});document.addEventListener('keydown',function(e){if(e.key==='Escape'){closeAbout();closeSidebar();closeCfgSheet();}});window.addEventListener('beforeunload',function(){clearTimeout(wsTimer);stopStatePoll();if(ws)ws.close();});setTimeout(redrawAll,100);window.addEventListener('resize',redrawAll);});