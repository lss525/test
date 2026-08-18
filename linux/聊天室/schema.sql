CREATE DATABASE IF NOT EXISTS chat_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE chat_system;

CREATE TABLE IF NOT EXISTS users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL,
    password_hash VARCHAR(64) NOT NULL,
    email VARCHAR(255) DEFAULT '',
    phone VARCHAR(32) DEFAULT '',
    nickname VARCHAR(64) DEFAULT '',
    status TINYINT DEFAULT 0,
    created_at BIGINT DEFAULT 0,
    UNIQUE KEY uk_username (username)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS verification_codes (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    target VARCHAR(255) NOT NULL,
    code VARCHAR(16) NOT NULL,
    type TINYINT DEFAULT 1,
    expires_at BIGINT DEFAULT 0,
    used TINYINT DEFAULT 0,
    created_at BIGINT DEFAULT 0,
    KEY idx_target (target)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS friendships (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id BIGINT NOT NULL,
    friend_id BIGINT NOT NULL,
    status TINYINT DEFAULT 0,
    blocked_by BIGINT DEFAULT NULL,
    created_at BIGINT DEFAULT 0,
    UNIQUE KEY uk_user_friend (user_id, friend_id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (friend_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS private_messages (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    message_id VARCHAR(36) NOT NULL,
    sender_id BIGINT NOT NULL,
    receiver_id BIGINT NOT NULL,
    content TEXT,
    message_type TINYINT DEFAULT 1,
    status TINYINT DEFAULT 0,
    sent_at BIGINT DEFAULT 0,
    FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (receiver_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS file_transfers (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    transfer_id VARCHAR(36) NOT NULL,
    sender_id BIGINT NOT NULL,
    receiver_id BIGINT DEFAULT 0,
    group_id BIGINT DEFAULT 0,
    file_name VARCHAR(255) NOT NULL,
    file_size BIGINT NOT NULL,
    file_path VARCHAR(500) NOT NULL,
    status TINYINT DEFAULT 0,
    notified TINYINT DEFAULT 0,
    created_at BIGINT DEFAULT 0,
    FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;
-- ========== 群组 ==========
CREATE TABLE IF NOT EXISTS groups_info (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    group_name VARCHAR(100) NOT NULL,
    owner_id BIGINT NOT NULL,
    description TEXT,          -- 去掉 DEFAULT ''
    created_at BIGINT DEFAULT 0,
    UNIQUE KEY uk_group_name (group_name),
    FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS group_members (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    group_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    role TINYINT DEFAULT 0,
    joined_at BIGINT DEFAULT 0,
    UNIQUE KEY uk_group_user (group_id, user_id),
    FOREIGN KEY (group_id) REFERENCES groups_info(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS group_messages (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    message_id VARCHAR(36) NOT NULL,
    group_id BIGINT NOT NULL,
    sender_id BIGINT NOT NULL,
    content TEXT,
    status TINYINT DEFAULT 0,
    sent_at BIGINT DEFAULT 0,
    FOREIGN KEY (group_id) REFERENCES groups_info(id) ON DELETE CASCADE,
    FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS group_join_requests (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    group_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    status TINYINT DEFAULT 0,
    message VARCHAR(255) DEFAULT '',
    created_at BIGINT DEFAULT 0,
    handled_at BIGINT DEFAULT 0,
    notified TINYINT DEFAULT 0,
    FOREIGN KEY (group_id) REFERENCES groups_info(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB;