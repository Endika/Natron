//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */

#include "FromQtEnums.h"


///what a painful mapping!
Natron::Key QtEnumConvert::fromQtKey(Qt::Key k) {
    switch (k) {
        case Qt::Key_Escape:
            return Natron::Key_Escape;
        case Qt::Key_Tab:
            return Natron::Key_Tab;
        case Qt::Key_Clear:
            return Natron::Key_Clear;
        case Qt::Key_Return:
            return Natron::Key_Return;
        case Qt::Key_Pause:
            return Natron::Key_Pause;
        case Qt::Key_Delete :
            return Natron::Key_Delete;
            
        case Qt::Key_Multi_key :
            return Natron::Key_Multi_key;
        case Qt::Key_SingleCandidate :
            return Natron::Key_SingleCandidate;
        case Qt::Key_MultipleCandidate :
            return Natron::Key_MultipleCandidate;
        case Qt::Key_PreviousCandidate :
            return Natron::Key_PreviousCandidate;
            
        case Qt::Key_Kanji :
            return Natron::Key_Kanji;
        case Qt::Key_Muhenkan :
            return Natron::Key_Muhenkan;
        case Qt::Key_Henkan :
            return Natron::Key_Henkan;
        case Qt::Key_Romaji :
            return Natron::Key_Romaji;
        case Qt::Key_Hiragana :
            return Natron::Key_Hiragana;
        case Qt::Key_Katakana :
            return Natron::Key_Katakana;
        case Qt::Key_Hiragana_Katakana :
            return Natron::Key_Hiragana_Katakana;
        case Qt::Key_Zenkaku :
            return Natron::Key_Zenkaku;
        case Qt::Key_Hankaku :
            return Natron::Key_Hankaku;
        case Qt::Key_Zenkaku_Hankaku :
            return Natron::Key_Zenkaku_Hankaku;
        case Qt::Key_Touroku :
            return Natron::Key_Touroku;
        case Qt::Key_Massyo :
            return Natron::Key_Massyo;
        case Qt::Key_Kana_Lock :
            return Natron::Key_Kana_Lock;
        case Qt::Key_Kana_Shift :
            return Natron::Key_Kana_Shift;
        case Qt::Key_Eisu_Shift :
            return Natron::Key_Eisu_Shift;
        case Qt::Key_Eisu_toggle :
            return Natron::Key_Eisu_toggle;
            
        case Qt::Key_Home :
            return Natron::Key_Home;
        case Qt::Key_Left :
            return Natron::Key_Left;
        case Qt::Key_Up :
            return Natron::Key_Up;
        case Qt::Key_Right :
            return Natron::Key_Right;
        case Qt::Key_Down :
            return Natron::Key_Down;
        case Qt::Key_End :
            return Natron::Key_End;
            
        case Qt::Key_Select :
            return Natron::Key_Select;
        case Qt::Key_Print :
            return Natron::Key_Print;
        case Qt::Key_Execute :
            return Natron::Key_Execute;
        case Qt::Key_Insert :
            return Natron::Key_Insert;
        case Qt::Key_Menu :
            return Natron::Key_Menu;
        case Qt::Key_Cancel :
            return Natron::Key_Cancel;
        case Qt::Key_Help :
            return Natron::Key_Help;
        case Qt::Key_Mode_switch :
            return Natron::Key_Mode_switch;
        case Qt::Key_F1 :
            return Natron::Key_F1;
        case Qt::Key_F2 :
            return Natron::Key_F2;
        case Qt::Key_F3 :
            return Natron::Key_F3;
        case Qt::Key_F4 :
            return Natron::Key_F4;
        case Qt::Key_F5 :
            return Natron::Key_F5;
        case Qt::Key_F6 :
            return Natron::Key_F6;
        case Qt::Key_F7 :
            return Natron::Key_F7;
        case Qt::Key_F8 :
            return Natron::Key_F8;
        case Qt::Key_F9 :
            return Natron::Key_F9;
        case Qt::Key_F10 :
            return Natron::Key_F10;
        case Qt::Key_F11 :
            return Natron::Key_F11;
        case Qt::Key_F12 :
            return Natron::Key_F12;
        case Qt::Key_F13 :
            return Natron::Key_F13;
        case Qt::Key_F14 :
            return Natron::Key_F14;
        case Qt::Key_F15 :
            return Natron::Key_F15;
        case Qt::Key_F16 :
            return Natron::Key_F16;
        case Qt::Key_F17 :
            return Natron::Key_F17;
        case Qt::Key_F18 :
            return Natron::Key_F18;
        case Qt::Key_F19 :
            return Natron::Key_F19;
        case Qt::Key_F20 :
            return Natron::Key_F20;
        case Qt::Key_F21 :
            return Natron::Key_F21;
        case Qt::Key_F22 :
            return Natron::Key_F22;
        case Qt::Key_F23 :
            return Natron::Key_F23;
        case Qt::Key_F24 :
            return Natron::Key_F24;
        case Qt::Key_F25 :
            return Natron::Key_F25;
        case Qt::Key_F26 :
            return Natron::Key_F26;
        case Qt::Key_F27 :
            return Natron::Key_F27;
        case Qt::Key_F28 :
            return Natron::Key_F28;
        case Qt::Key_F29 :
            return Natron::Key_F29;
        case Qt::Key_F30 :
            return Natron::Key_F30;
        case Qt::Key_F31 :
            return Natron::Key_F31;
        case Qt::Key_F32 :
            return Natron::Key_F32;
        case Qt::Key_F33 :
            return Natron::Key_F33;
        case Qt::Key_F34 :
            return Natron::Key_F34;
        case Qt::Key_F35 :
            return Natron::Key_F35;
            
            
        case Qt::Key_Shift :
            return Natron::Key_Shift_L;
        case Qt::Key_Control :
            return Natron::Key_Control_L;
        case Qt::Key_CapsLock :
            return Natron::Key_Caps_Lock;
            
        case Qt::Key_Meta :
            return Natron::Key_Meta_L;
        case Qt::Key_Alt :
            return Natron::Key_Alt_L;
        case Qt::Key_Super_L :
            return Natron::Key_Super_L;
        case Qt::Key_Super_R :
            return Natron::Key_Super_R;
        case Qt::Key_Hyper_L :
            return Natron::Key_Hyper_L;
        case Qt::Key_Hyper_R :
            return Natron::Key_Hyper_R;
            
        case Qt::Key_Space :
            return Natron::Key_space;
        case Qt::Key_Exclam :
            return Natron::Key_exclam;
        case Qt::Key_QuoteDbl :
            return Natron::Key_quotedbl;
        case Qt::Key_NumberSign :
            return Natron::Key_numbersign;
        case Qt::Key_Dollar :
            return Natron::Key_dollar;
        case Qt::Key_Percent :
            return Natron::Key_percent;
        case Qt::Key_Ampersand :
            return Natron::Key_ampersand;
        case Qt::Key_Apostrophe :
            return Natron::Key_apostrophe;
        case Qt::Key_ParenLeft :
            return Natron::Key_parenleft;
        case Qt::Key_ParenRight :
            return Natron::Key_parenright;
        case Qt::Key_Asterisk :
            return Natron::Key_asterisk;
        case Qt::Key_Plus :
            return Natron::Key_plus;
        case Qt::Key_Comma :
            return Natron::Key_comma;
        case Qt::Key_Minus :
            return Natron::Key_minus;
        case Qt::Key_Period :
            return Natron::Key_period;
        case Qt::Key_Slash :
            return Natron::Key_slash;
        case Qt::Key_0 :
            return Natron::Key_0;
        case Qt::Key_1 :
            return Natron::Key_1;
        case Qt::Key_2 :
            return Natron::Key_2;
        case Qt::Key_3 :
            return Natron::Key_3;
        case Qt::Key_4 :
            return Natron::Key_4;
        case Qt::Key_5 :
            return Natron::Key_5;
        case Qt::Key_6 :
            return Natron::Key_6;
        case Qt::Key_7 :
            return Natron::Key_7;
        case Qt::Key_8 :
            return Natron::Key_8;
        case Qt::Key_9 :
            return Natron::Key_9;
        case Qt::Key_Colon :
            return Natron::Key_colon;
        case Qt::Key_Semicolon :
            return Natron::Key_semicolon;
        case Qt::Key_Less :
            return Natron::Key_less;
        case Qt::Key_Equal :
            return Natron::Key_equal;
        case Qt::Key_Greater :
            return Natron::Key_greater;
        case Qt::Key_Question :
            return Natron::Key_question;
        case Qt::Key_At :
            return Natron::Key_at;
        case Qt::Key_A :
            return Natron::Key_A;
        case Qt::Key_B :
            return Natron::Key_B;
        case Qt::Key_C :
            return Natron::Key_C;
        case Qt::Key_D :
            return Natron::Key_D;
        case Qt::Key_E :
            return Natron::Key_E;
        case Qt::Key_F :
            return Natron::Key_F;
        case Qt::Key_G :
            return Natron::Key_G;
        case Qt::Key_H :
            return Natron::Key_H;
        case Qt::Key_I :
            return Natron::Key_I;
        case Qt::Key_J :
            return Natron::Key_J;
        case Qt::Key_K :
            return Natron::Key_K;
        case Qt::Key_L :
            return Natron::Key_L;
        case Qt::Key_M :
            return Natron::Key_M;
        case Qt::Key_N :
            return Natron::Key_N;
        case Qt::Key_O :
            return Natron::Key_O;
        case Qt::Key_P :
            return Natron::Key_P;
        case Qt::Key_Q :
            return Natron::Key_Q;
        case Qt::Key_R :
            return Natron::Key_R;
        case Qt::Key_S :
            return Natron::Key_S;
        case Qt::Key_T :
            return Natron::Key_T;
        case Qt::Key_U :
            return Natron::Key_U;
        case Qt::Key_V :
            return Natron::Key_V;
        case Qt::Key_W :
            return Natron::Key_W;
        case Qt::Key_X :
            return Natron::Key_X;
        case Qt::Key_Y :
            return Natron::Key_Y;
        case Qt::Key_Z :
            return Natron::Key_Z;
        case Qt::Key_BracketLeft :
            return Natron::Key_bracketleft;
        case Qt::Key_Backslash :
            return Natron::Key_backslash;
        case Qt::Key_BracketRight :
            return Natron::Key_bracketright;
        case Qt::Key_AsciiCircum :
            return Natron::Key_asciicircum;
        case Qt::Key_Underscore :
            return Natron::Key_underscore;
        case Qt::Key_QuoteLeft :
            return Natron::Key_quoteleft;
        case Qt::Key_BraceLeft :
            return Natron::Key_braceleft;
        case Qt::Key_Bar :
            return Natron::Key_bar;
        case Qt::Key_BraceRight :
            return Natron::Key_braceright;
        case Qt::Key_AsciiTilde :
            return Natron::Key_asciitilde;
            
        case Qt::Key_nobreakspace :
            return Natron::Key_nobreakspace;
        case Qt::Key_exclamdown :
            return Natron::Key_exclamdown;
        case Qt::Key_cent :
            return Natron::Key_cent;
        case Qt::Key_sterling :
            return Natron::Key_sterling;
        case Qt::Key_currency :
            return Natron::Key_currency;
        case Qt::Key_yen :
            return Natron::Key_yen;
        case Qt::Key_brokenbar :
            return Natron::Key_brokenbar;
        case Qt::Key_section :
            return Natron::Key_section;
        case Qt::Key_diaeresis :
            return Natron::Key_diaeresis;
        case Qt::Key_copyright :
            return Natron::Key_copyright;
        case Qt::Key_ordfeminine :
            return Natron::Key_ordfeminine;
        case Qt::Key_guillemotleft :
            return Natron::Key_guillemotleft;
        case Qt::Key_notsign :
            return Natron::Key_notsign;
        case Qt::Key_hyphen :
            return Natron::Key_hyphen;
        case Qt::Key_registered :
            return Natron::Key_registered;
        case Qt::Key_macron :
            return Natron::Key_macron;
        case Qt::Key_degree :
            return Natron::Key_degree;
        case Qt::Key_plusminus :
            return Natron::Key_plusminus;
        case Qt::Key_twosuperior :
            return Natron::Key_twosuperior;
        case Qt::Key_threesuperior :
            return Natron::Key_threesuperior;
        case Qt::Key_acute :
            return Natron::Key_acute;
        case Qt::Key_mu :
            return Natron::Key_mu;
        case Qt::Key_paragraph :
            return Natron::Key_paragraph;
        case Qt::Key_periodcentered :
            return Natron::Key_periodcentered;
        case Qt::Key_cedilla :
            return Natron::Key_cedilla;
        case Qt::Key_onesuperior :
            return Natron::Key_onesuperior;
        case Qt::Key_masculine :
            return Natron::Key_masculine;
        case Qt::Key_guillemotright :
            return Natron::Key_guillemotright;
        case Qt::Key_onequarter :
            return Natron::Key_onequarter;
        case Qt::Key_onehalf :
            return Natron::Key_onehalf;
        case Qt::Key_threequarters :
            return Natron::Key_threequarters;
        case Qt::Key_questiondown :
            return Natron::Key_questiondown;
        case Qt::Key_Agrave :
            return Natron::Key_Agrave;
        case Qt::Key_Aacute :
            return Natron::Key_Aacute;
        case Qt::Key_Acircumflex :
            return Natron::Key_Acircumflex;
        case Qt::Key_Atilde :
            return Natron::Key_Atilde;
        case Qt::Key_Adiaeresis :
            return Natron::Key_Adiaeresis;
        case Qt::Key_Aring :
            return Natron::Key_Aring;
        case Qt::Key_AE :
            return Natron::Key_AE;
        case Qt::Key_Ccedilla :
            return Natron::Key_Ccedilla;
        case Qt::Key_Egrave :
            return Natron::Key_Egrave;
        case Qt::Key_Eacute :
            return Natron::Key_Eacute;
        case Qt::Key_Ecircumflex :
            return Natron::Key_Ecircumflex;
        case Qt::Key_Ediaeresis :
            return Natron::Key_Ediaeresis;
        case Qt::Key_Igrave :
            return Natron::Key_Igrave;
        case Qt::Key_Iacute :
            return Natron::Key_Iacute;
        case Qt::Key_Icircumflex :
            return Natron::Key_Icircumflex;
        case Qt::Key_Idiaeresis :
            return Natron::Key_Idiaeresis;
        case Qt::Key_ETH :
            return Natron::Key_ETH;
        case Qt::Key_Ntilde :
            return Natron::Key_Ntilde;
        case Qt::Key_Ograve :
            return Natron::Key_Ograve;
        case Qt::Key_Oacute :
            return Natron::Key_Oacute;
        case Qt::Key_Ocircumflex :
            return Natron::Key_Ocircumflex;
        case Qt::Key_Otilde :
            return Natron::Key_Otilde;
        case Qt::Key_Odiaeresis :
            return Natron::Key_Odiaeresis;
        case Qt::Key_multiply :
            return Natron::Key_multiply;
        case Qt::Key_Ooblique :
            return Natron::Key_Ooblique;
        case Qt::Key_Ugrave :
            return Natron::Key_Ugrave;
        case Qt::Key_Uacute :
            return Natron::Key_Uacute;
        case Qt::Key_Ucircumflex :
            return Natron::Key_Ucircumflex;
        case Qt::Key_Udiaeresis :
            return Natron::Key_Udiaeresis;
        case Qt::Key_Yacute :
            return Natron::Key_Yacute;
        case Qt::Key_ssharp :
            return Natron::Key_ssharp;
        case Qt::Key_THORN :
            return Natron::Key_thorn;
        case Qt::Key_ydiaeresis :
            return Natron::Key_ydiaeresis;
        default:
            return Natron::Key_Unknown;
    }
}

Natron::KeyboardModifier
QtEnumConvert::fromQtModifier(Qt::KeyboardModifier m)
{
    switch (m) {
        case Qt::NoModifier :
            return Natron::NoModifier;
            break;
        case Qt::ShiftModifier :
            return Natron::ShiftModifier;
            break;
        case Qt::ControlModifier :
            return Natron::ControlModifier;
            break;
        case Qt::AltModifier :
            return Natron::AltModifier;
            break;
        case Qt::MetaModifier :
            return Natron::MetaModifier;
            break;
        case Qt::KeypadModifier :
            return Natron::KeypadModifier;
            break;
        case Qt::GroupSwitchModifier :
            return Natron::GroupSwitchModifier;
            break;
        case Qt::KeyboardModifierMask :
            return Natron::KeyboardModifierMask;
            break;
        default:
            return Natron::NoModifier;
            break;
    }
}

Natron::KeyboardModifiers
QtEnumConvert::fromQtModifiers(Qt::KeyboardModifiers m)
{
    Natron::KeyboardModifiers ret;
    if (m.testFlag(Qt::NoModifier)) {
        ret |= fromQtModifier(Qt::NoModifier);
    }
    if (m.testFlag(Qt::ShiftModifier)) {
        ret |= fromQtModifier(Qt::ShiftModifier);
    }
    if (m.testFlag(Qt::ControlModifier)) {
        ret |= fromQtModifier(Qt::ControlModifier);
    }
    if (m.testFlag(Qt::AltModifier)) {
        ret |= fromQtModifier(Qt::AltModifier);
    }
    if (m.testFlag(Qt::MetaModifier)) {
        ret |= fromQtModifier(Qt::MetaModifier);
    }
    if (m.testFlag(Qt::KeypadModifier)) {
        ret |= fromQtModifier(Qt::KeypadModifier);
    }
    if (m.testFlag(Qt::GroupSwitchModifier)) {
        ret |= fromQtModifier(Qt::GroupSwitchModifier);
    }
    if (m.testFlag(Qt::KeyboardModifierMask)) {
        ret |= fromQtModifier(Qt::KeyboardModifierMask);
    }
    
    return ret;
}

Natron::StandardButton QtEnumConvert::fromQtStandardButton(QMessageBox::StandardButton b) {
    switch (b) {
        case QMessageBox::NoButton:
            return Natron::NoButton;
            break;
        case QMessageBox::Escape:
            return Natron::Escape;
            break;
        case QMessageBox::Ok:
            return Natron::Ok;
            break;
        case QMessageBox::Save:
            return Natron::Save;
            break;
        case QMessageBox::SaveAll:
            return Natron::SaveAll;
            break;
        case QMessageBox::Open:
            return Natron::Open;
            break;
        case QMessageBox::Yes:
            return Natron::Yes;
            break;
        case QMessageBox::YesToAll:
            return Natron::YesToAll;
            break;
        case QMessageBox::No:
            return Natron::No;
            break;
        case QMessageBox::NoToAll:
            return Natron::NoToAll;
            break;
        case QMessageBox::Abort:
            return Natron::Abort;
            break;
        case QMessageBox::Retry:
            return Natron::Retry;
            break;
        case QMessageBox::Ignore:
            return Natron::Ignore;
            break;
        case QMessageBox::Close:
            return Natron::Close;
            break;
        case QMessageBox::Cancel:
            return Natron::Cancel;
            break;
        case QMessageBox::Discard:
            return Natron::Discard;
            break;
        case QMessageBox::Help:
            return Natron::Help;
            break;
        case QMessageBox::Apply:
            return Natron::Apply;
            break;
        case QMessageBox::Reset:
            return Natron::Reset;
            break;
        case QMessageBox::RestoreDefaults:
            return Natron::RestoreDefaults;
            break;
        default:
            return Natron::NoButton;
            break;
    }
}

QMessageBox::StandardButton QtEnumConvert::toQtStandardButton(Natron::StandardButton b) {
    switch (b) {
        case Natron::NoButton:
            return QMessageBox::NoButton;
            break;
        case Natron::Escape:
            return QMessageBox::Escape;
            break;
        case Natron::Ok:
            return QMessageBox::Ok;
            break;
        case Natron::Save:
            return QMessageBox::Save;
            break;
        case Natron::SaveAll:
            return QMessageBox::SaveAll;
            break;
        case Natron::Open:
            return QMessageBox::Open;
            break;
        case Natron::Yes:
            return QMessageBox::Yes;
            break;
        case Natron::YesToAll:
            return QMessageBox::YesToAll;
            break;
        case Natron::No:
            return QMessageBox::No;
            break;
        case Natron::NoToAll:
            return QMessageBox::NoToAll;
            break;
        case Natron::Abort:
            return QMessageBox::Abort;
            break;
        case Natron::Retry:
            return QMessageBox::Retry;
            break;
        case Natron::Ignore:
            return QMessageBox::Ignore;
            break;
        case Natron::Close:
            return QMessageBox::Close;
            break;
        case Natron::Cancel:
            return QMessageBox::Cancel;
            break;
        case Natron::Discard:
            return QMessageBox::Discard;
            break;
        case Natron::Help:
            return QMessageBox::Help;
            break;
        case Natron::Apply:
            return QMessageBox::Apply;
            break;
        case Natron::Reset:
            return QMessageBox::Reset;
            break;
        case Natron::RestoreDefaults:
            return QMessageBox::RestoreDefaults;
            break;
        default:
            return QMessageBox::NoButton;
            break;
    }
}



QMessageBox::StandardButtons
QtEnumConvert::toQtStandarButtons(Natron::StandardButtons buttons)
{
    QMessageBox::StandardButtons ret;
    if (buttons.testFlag(Natron::NoButton)) {
        ret |= QMessageBox::NoButton;
    }
    if (buttons.testFlag(Natron::Escape)) {
        ret |= QMessageBox::Escape;
    }
    if (buttons.testFlag(Natron::Ok)) {
        ret |= QMessageBox::Ok;
    }
    if (buttons.testFlag(Natron::Save)) {
        ret |= QMessageBox::Save;
    }
    if (buttons.testFlag(Natron::SaveAll)) {
        ret |= QMessageBox::SaveAll;
    }
    if (buttons.testFlag(Natron::Open)) {
        ret |= QMessageBox::Open;
    }
    if (buttons.testFlag(Natron::Yes)) {
        ret |= QMessageBox::Yes;
    }
    if (buttons.testFlag(Natron::YesToAll)) {
        ret |= QMessageBox::YesToAll;
    }
    if (buttons.testFlag(Natron::No)) {
        ret |= QMessageBox::No;
    }
    if (buttons.testFlag(Natron::NoToAll)) {
        ret |= QMessageBox::NoToAll;
    }
    if (buttons.testFlag(Natron::Abort)) {
        ret |= QMessageBox::Abort;
    }
    if (buttons.testFlag(Natron::Ignore)) {
        ret |= QMessageBox::Ignore;
    }
    if (buttons.testFlag(Natron::Retry)) {
        ret |= QMessageBox::Retry;
    }
    if (buttons.testFlag(Natron::Close)) {
        ret |= QMessageBox::Close;
    }
    if (buttons.testFlag(Natron::Cancel)) {
        ret |= QMessageBox::Cancel;
    }
    if (buttons.testFlag(Natron::Discard)) {
        ret |= QMessageBox::Discard;
    }
    if (buttons.testFlag(Natron::Help)) {
        ret |= QMessageBox::Help;
    }
    if (buttons.testFlag(Natron::Apply)) {
        ret |= QMessageBox::Apply;
    }
    if (buttons.testFlag(Natron::Reset)) {
        ret |= QMessageBox::Reset;
    }
    if (buttons.testFlag(Natron::RestoreDefaults)) {
        ret |= QMessageBox::RestoreDefaults;
    }
    return ret;
}

