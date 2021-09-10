//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "console/address_book_dialog.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "base/strings/unicode.h"

#include <QAbstractButton>
#include <QMessageBox>

namespace console {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMinPasswordLength = 1;
constexpr int kMaxPasswordLength = 64;
constexpr int kSafePasswordLength = 8;
constexpr int kMaxCommentLength = 2048;
constexpr int kHashingSaltSize = 256;

bool isSafePassword(const QString& password)
{
    int length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (int i = 0; i < length; ++i)
    {
        const QChar& character = password[i];

        has_upper |= character.isUpper();
        has_lower |= character.isLower();
        has_digit |= character.isDigit();
    }

    return has_upper && has_lower && has_digit;
}

} // namespace

AddressBookDialog::AddressBookDialog(QWidget* parent,
                                     const QString& file_path,
                                     proto::address_book::File* file,
                                     proto::address_book::Data* data,
                                     std::string* key)
    : QDialog(parent),
      file_(file),
      data_(data),
      key_(key)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &AddressBookDialog::buttonBoxClicked);

    connect(ui.combo_encryption, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AddressBookDialog::encryptionTypedChanged);

    ui.combo_encryption->addItem(tr("Without Encryption"),
                                 QVariant(proto::address_book::ENCRYPTION_TYPE_NONE));
    ui.combo_encryption->addItem(tr("ChaCha20 + Poly1305 (256-bit key)"),
                                 QVariant(proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305));

    const std::string& address_book_name = data_->root_group().name();
    if (!address_book_name.empty())
    {
        // Use the name of the root group of computers.
        ui.edit_name->setText(QString::fromStdString(address_book_name));
    }
    else
    {
        // Set the default address book name.
        ui.edit_name->setText(tr("Address Book"));
    }

    ui.edit_file->setText(file_path);
    if (file_path.isEmpty())
        ui.edit_file->setEnabled(false);

    ui.edit_comment->setPlainText(QString::fromStdString(data_->root_group().comment()));

    int current = ui.combo_encryption->findData(QVariant(file->encryption_type()));
    if (current != -1)
        ui.combo_encryption->setCurrentIndex(current);

    if (file->encryption_type() == proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305)
    {
        if (!key_->empty())
        {
            QString text = tr("Double-click to change");

            ui.edit_password->setText(text);
            ui.edit_password_repeat->setText(text);

            ui.edit_password->setEnabled(false);
            ui.edit_password_repeat->setEnabled(false);

            ui.edit_password->setEchoMode(QLineEdit::Normal);
            ui.edit_password->setInputMethodHints(Qt::ImhNone);

            ui.edit_password_repeat->setEchoMode(QLineEdit::Normal);
            ui.edit_password_repeat->setInputMethodHints(Qt::ImhNone);

            ui.edit_password->installEventFilter(this);
            ui.edit_password_repeat->installEventFilter(this);

            password_changed_ = false;
        }
    }
    else
    {
        DCHECK_EQ(file->encryption_type(), proto::address_book::ENCRYPTION_TYPE_NONE);

        ui.edit_password->setEnabled(false);

        // Disable Advanced tab.
        ui.tab_widget->setTabEnabled(2, false);
    }

    const proto::address_book::Router& router = data_->router();

    base::Address address(DEFAULT_ROUTER_TCP_PORT);
    address.setHost(base::utf16FromUtf8(router.address()));
    address.setPort(static_cast<uint16_t>(router.port()));

    bool enable_router = data_->enable_router();
    ui.checkbox_use_router->setChecked(enable_router);

    ui.label_router_address->setEnabled(enable_router);
    ui.edit_router_address->setEnabled(enable_router);
    ui.label_router_username->setEnabled(enable_router);
    ui.edit_router_username->setEnabled(enable_router);
    ui.label_router_password->setEnabled(enable_router);
    ui.edit_router_password->setEnabled(enable_router);
    ui.button_show_password->setEnabled(enable_router);

    ui.edit_router_address->setText(QString::fromStdU16String(address.toString()));
    ui.edit_router_username->setText(QString::fromStdString(router.username()));
    ui.edit_router_password->setText(QString::fromStdString(router.password()));

    connect(ui.button_show_password, &QPushButton::toggled, this, [this](bool checked)
    {
        if (checked)
        {
            ui.edit_router_password->setEchoMode(QLineEdit::Normal);
            ui.edit_router_password->setInputMethodHints(Qt::ImhNone);
        }
        else
        {
            ui.edit_router_password->setEchoMode(QLineEdit::Password);
            ui.edit_router_password->setInputMethodHints(
                Qt::ImhHiddenText | Qt::ImhSensitiveData |
                Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
        }

        ui.edit_router_password->setFocus();
    });

    connect(ui.checkbox_use_router, &QCheckBox::toggled, this, [this](bool checked)
    {
        ui.label_router_address->setEnabled(checked);
        ui.edit_router_address->setEnabled(checked);

        ui.label_router_username->setEnabled(checked);
        ui.edit_router_username->setEnabled(checked);

        ui.label_router_password->setEnabled(checked);
        ui.edit_router_password->setEnabled(checked);
        ui.button_show_password->setEnabled(checked);
    });

    ui.edit_name->setFocus();
}

bool AddressBookDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui.edit_password || object == ui.edit_password_repeat))
    {
        proto::address_book::EncryptionType encryption_type =
            static_cast<proto::address_book::EncryptionType>(
                ui.combo_encryption->currentData().toInt());

        if (encryption_type == proto::address_book::ENCRYPTION_TYPE_NONE)
            return false;

        setPasswordChanged();
    }

    return false;
}

void AddressBookDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        close();
        return;
    }

    QString name = ui.edit_name->text();
    if (name.length() > kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", kMaxNameLength));
        return;
    }
    else if (name.length() < kMinNameLength)
    {
        showError(tr("Name can not be empty."));
        return;
    }

    QString comment = ui.edit_comment->toPlainText();
    if (comment.length() > kMaxCommentLength)
    {
        showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                     "", kMaxCommentLength));
        return;
    }

    proto::address_book::EncryptionType encryption_type =
        static_cast<proto::address_book::EncryptionType>(
            ui.combo_encryption->currentData().toInt());

    switch (encryption_type)
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
        {
            file_->mutable_hashing_salt()->clear();
        }
        break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            if (password_changed_)
            {
                QString password = ui.edit_password->text();
                QString password_repeat = ui.edit_password_repeat->text();

                if (password != password_repeat)
                {
                    showError(tr("The passwords you entered do not match."));
                    return;
                }

                if (password.length() < kMinPasswordLength || password.length() > kMaxPasswordLength)
                {
                    showError(tr("Password can not be empty and should not exceed %n characters.",
                                 "", kMaxPasswordLength));
                    return;
                }

                if (!isSafePassword(password))
                {
                    QString unsafe =
                        tr("Password you entered does not meet the security requirements!");

                    QString safe =
                        tr("The password must contain lowercase and uppercase characters, "
                           "numbers and should not be shorter than %n characters.",
                           "", kSafePasswordLength);

                    QString question = tr("Do you want to enter a different password?");

                    if (QMessageBox::warning(this,
                                             tr("Warning"),
                                             QString("<b>%1</b><br/>%2<br/>%3")
                                                 .arg(unsafe, safe, question),
                                             QMessageBox::Yes,
                                             QMessageBox::No) == QMessageBox::Yes)
                    {
                        ui.edit_password->clear();
                        ui.edit_password_repeat->clear();
                        ui.edit_password->setFocus();
                        return;
                    }
                }

                // Generate salt, which is added after each iteration of the hashing.
                // New salt is generated each time the password is changed.
                file_->set_hashing_salt(base::Random::string(kHashingSaltSize));

                // Now generate a key for encryption/decryption.
                *key_ = base::PasswordHash::hash(
                    base::PasswordHash::SCRYPT, password.toStdString(), file_->hashing_salt());
            }
        }
        break;

        default:
            LOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type;
            return;
    }

    if (ui.checkbox_use_router->isChecked())
    {
        base::Address address = base::Address::fromString(
            ui.edit_router_address->text().toStdU16String(), DEFAULT_ROUTER_TCP_PORT);
        if (!address.isValid())
        {
            showError(tr("An invalid router address was entered."));
            ui.edit_router_address->setFocus();
            ui.edit_router_address->selectAll();
            return;
        }

        std::u16string username = ui.edit_router_username->text().toStdU16String();
        std::u16string password = ui.edit_router_password->text().toStdU16String();

        if (!base::User::isValidUserName(username))
        {
            showError(tr("The user name can not be empty and can contain only"
                         " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
            ui.edit_router_username->setFocus();
            ui.edit_router_username->selectAll();
            return;
        }

        if (!base::User::isValidPassword(password))
        {
            showError(tr("Router password cannot be empty."));
            ui.edit_router_password->setFocus();
            ui.edit_router_password->selectAll();
            return;
        }

        proto::address_book::Router* router = data_->mutable_router();
        router->set_address(base::utf8FromUtf16(address.host()));
        router->set_port(address.port());
        router->set_username(base::utf8FromUtf16(username));
        router->set_password(base::utf8FromUtf16(password));
    }

    file_->set_encryption_type(encryption_type);
    data_->mutable_root_group()->set_name(name.toStdString());
    data_->mutable_root_group()->set_comment(comment.toStdString());
    data_->set_enable_router(ui.checkbox_use_router->isChecked());

    if (data_->guid().empty())
        data_->set_guid(base::Guid::create().toStdString());

    accept();
    close();
}

void AddressBookDialog::encryptionTypedChanged(int item_index)
{
    proto::address_book::EncryptionType encryption_type =
        static_cast<proto::address_book::EncryptionType>(
            ui.combo_encryption->itemData(item_index).toInt());

    switch (encryption_type)
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
        {
            ui.edit_password->setEnabled(false);
            ui.edit_password_repeat->setEnabled(false);

            ui.edit_password->clear();
            ui.edit_password_repeat->clear();

            // Disable Advanced tab.
            ui.tab_widget->setTabEnabled(2, false);
        }
        break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            ui.edit_password->setEnabled(true);
            ui.edit_password_repeat->setEnabled(true);

            // Enable Advanced tab.
            ui.tab_widget->setTabEnabled(2, true);
        }
        break;

        default:
            LOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type;
            break;
    }
}

void AddressBookDialog::setPasswordChanged()
{
    password_changed_ = true;

    ui.edit_password->setEnabled(true);
    ui.edit_password_repeat->setEnabled(true);

    ui.edit_password->clear();
    ui.edit_password_repeat->clear();

    Qt::InputMethodHints hints = Qt::ImhHiddenText | Qt::ImhSensitiveData |
        Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;

    ui.edit_password->setEchoMode(QLineEdit::Password);
    ui.edit_password->setInputMethodHints(hints);

    ui.edit_password_repeat->setEchoMode(QLineEdit::Password);
    ui.edit_password_repeat->setInputMethodHints(hints);

    ui.edit_password->setFocus();
}

void AddressBookDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace console
