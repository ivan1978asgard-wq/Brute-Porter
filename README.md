# SSH Auth Checker

простой C++ инструмент для проверки SSH-авторизации заданной парой `username/password`.

## Что делает

- Проверяет SSH-авторизацию для одного хоста или списка хостов
- Не выполняет brute-force
- Показывает только счётчики:
  - `check: <кол-во проверок>  valid: <успешных>  bad: <неуспешных>`

## Зависимости

- C++11+
- `libssh`

### Debian/Ubuntu

```bash
sudo apt install libssh-dev build-essential
```

## Сборка

```bash
g++ brute-porter.cpp -o brute-porter -lssh -pthread
```

## Запуск

```bash
./brute-porter
```

## Использование

1. Выберите режим:
   - `1` — один хост
   - `2` — файл со списком хостов (по одному в строке)
2. Укажите порт SSH
3. Укажите `username` и `password`
4. Укажите количество потоков для проверки
5. Получите итоговую статистику в формате `check/valid/bad`
