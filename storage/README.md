# Database

### Models

##### Miner info
```
CREATE TABLE miner_info_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,  -- Уникальный идентификатор записи
    timestamp   DATETIME NOT NULL,                  -- Время фиксации данных (в ISO-формате)
    hashrate    DOUBLE  NOT NULL,                   -- Хэшрейт (MH/s)
    utility     DOUBLE  NOT NULL,                   -- Полезность (доли/мин)
    power       DOUBLE  NOT NULL,                   -- Потребляемая мощность 
    voltage     DOUBLE  NOT NULL,                   -- Напряжение (В)
    uptime      INTEGER NOT NULL                    -- Время работы (сек)
)
```