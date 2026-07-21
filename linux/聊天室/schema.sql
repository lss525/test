CREATE DATABASE IF NOT EXISTS chat_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE chat_system;

CREATE TABLE users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(100) DEFAULT '',
    phone VARCHAR(20) DEFAULT '',
    nickname VARCHAR(50) DEFAULT '',
    status TINYINT DEFAULT 0,
    created_at BIGINT DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE verification_codes (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    target VARCHAR(100) NOT NULL,
    code VARCHAR(10) NOT NULL,
    type TINYINT NOT NULL,
    used TINYINT DEFAULT 0,
    expires_at BIGINT NOT NULL
) ENGINE=InnoDB;