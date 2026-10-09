# Приёмник анонимной статистики и админ-страница

Небольшое приложение на FastAPI + SQLite. Принимает сообщения контроллеров
(`POST /api/v1/hb`, раз в час) и показывает владельцу таблицу устройств
(`/admin`, под паролем). Внешний IP и страну определяет **сервер** по самому
соединению; контроллер их не отправляет.

## Установка на Ubuntu

```bash
sudo apt install python3-venv nginx certbot python3-certbot-nginx
sudo useradd --system --home /opt/gh-telemetry --shell /usr/sbin/nologin ghtlm
sudo mkdir -p /opt/gh-telemetry && sudo cp app.py requirements.txt /opt/gh-telemetry/
cd /opt/gh-telemetry
sudo python3 -m venv venv && sudo venv/bin/pip install -r requirements.txt
sudo chown -R ghtlm: /opt/gh-telemetry

# хэш пароля админа (не короче 12 символов); результат вставить в gh-telemetry.env
sudo venv/bin/python app.py hash
```

Файл окружения `/etc/gh-telemetry.env` (права `600`, владелец root):

```
GH_DB=/opt/gh-telemetry/telemetry.db
GH_ADMIN_USER=admin
GH_ADMIN_HASH=pbkdf2_sha256$200000$...$...
GH_GEOIP=/opt/gh-telemetry/GeoLite2-Country.mmdb
GH_TRUST_PROXY=1
```

Служба и nginx — готовые файлы `gh-telemetry.service` и `nginx.conf.example`:

```bash
sudo cp gh-telemetry.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now gh-telemetry
# nginx: подставить свой домен, затем
sudo certbot --nginx -d pf.kzpm.org
```

## Страны

Нужна бесплатная база MaxMind **GeoLite2-Country** (регистрация на maxmind.com,
файл `.mmdb`) и пакет `geoip2` (`venv/bin/pip install geoip2`). Без базы страна
показывается как `?`, остальное работает.

## Прошивка

В сборку вписывается адрес приёмника (только `https://`):

```ini
build_flags = ... -D TELEMETRY_URL=\"https://pf.kzpm.org/api/v1/hb\"
```

Пока адрес пуст, функции в сборке нет. Пользователь включает отправку сам:
«Настройки → Система → Анонимная статистика».

## Что хранится

Таблица `devices` — последнее состояние каждого устройства; `events` — история
изменений (новый IP, страна, версия, часовой пояс, перезапуск, возвращение на
связь), хранится 2 года. Показания датчиков, имена сетей и пароли не
передаются и не хранятся.

## Безопасность

* Админка — Basic-авторизация, пароль только в виде хэша PBKDF2; 5 неудач за
  5 минут с одного адреса — блокировка на 5 минут. Работать только через HTTPS.
* `X-Real-IP` принимается лишь от `127.0.0.1` (свой nginx).
* Входящие поля проверяются строго по формату; страницы экранируют весь вывод
  и отдаются с `Content-Security-Policy`.
* Подделать отчёт может любой, кто знает формат (ключа в публичной прошивке
  не спрятать). Для статистики это допустимо; для платных функций понадобится
  регистрация устройства с выдачей токена.

## Персональные данные

IP-адрес считается персональными данными (152-ФЗ, GDPR). Нужны: понятное
пользователю описание (есть в README прошивки и в панели), согласие (тумблер
выключен по умолчанию), возможность удалить данные устройства по запросу
(`DELETE FROM devices WHERE id=...; DELETE FROM events WHERE device=...;`).
