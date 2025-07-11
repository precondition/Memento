////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021 Ripose
//
// This file is part of Memento.
//
// Memento is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.
//
// Memento is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Memento.  If not, see <https://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "kanjiwidget.h"

#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QHBoxLayout>

#include "anki/ankiclient.h"
#include "gui/widgets/common/flowlayout.h"
#include "gui/widgets/definition/tagwidget.h"
#include "gui/widgets/subtitlelistwidget.h"
#include "player/playeradapter.h"
#include "util/iconfactory.h"

/* Begin Constructor/Destructor */

KanjiWidget::KanjiWidget(
    Context *context,
    std::shared_ptr<const Kanji> kanji,
    bool showBack,
    QWidget *parent) :
    QWidget(parent),
    m_context(std::move(context)),
    m_kanji(std::move(kanji))
{
    setFocusPolicy(Qt::ClickFocus);

    QVBoxLayout *layoutParent = new QVBoxLayout(this);

    QHBoxLayout *layoutTop = new QHBoxLayout;
    layoutParent->addLayout(layoutTop);

    IconFactory *factory = IconFactory::create();

    if (showBack)
    {
        QToolButton *buttonBack = new QToolButton;
        buttonBack->setIcon(factory->getIcon(IconFactory::Icon::back));
        buttonBack->setMinimumSize(QSize(30, 30));
        buttonBack->setToolTip("Return to search results");
        connect(
            buttonBack, &QToolButton::clicked, this, &KanjiWidget::backPressed
        );
        layoutTop->addWidget(buttonBack);
        layoutTop->setAlignment(buttonBack, Qt::AlignTop | Qt::AlignLeft);
    }

    QLabel *labelKanjiStroke = new QLabel;
    labelKanjiStroke->setText(m_kanji->character);
    labelKanjiStroke->setStyleSheet(
        "QLabel {"
            "font-family: \"KanjiStrokeOrders\", \"Noto Sans\", \"Noto Sans JP\", \"Noto Sans CJK JP\", sans-serif;"
#if defined(Q_OS_MACOS)
            "font-size: 140pt;"
#else
            "font-size: 100pt;"
#endif
        "}"
    );
    labelKanjiStroke->setAlignment(Qt::AlignHCenter);
    labelKanjiStroke->setTextInteractionFlags(Qt::TextSelectableByMouse);
    labelKanjiStroke->setSizePolicy(
        QSizePolicy::Preferred, QSizePolicy::Preferred
    );
    layoutTop->addWidget(labelKanjiStroke);
    layoutTop->setAlignment(labelKanjiStroke, Qt::AlignTop | Qt::AlignCenter);
    layoutTop->addStretch();

    m_buttonAnkiAddOpen = new QToolButton;
    m_buttonAnkiAddOpen->setMinimumSize(QSize(30, 30));
    m_buttonAnkiAddOpen->hide();
    layoutTop->addWidget(m_buttonAnkiAddOpen);
    layoutTop->setAlignment(m_buttonAnkiAddOpen, Qt::AlignTop | Qt::AlignRight);

    m_shortcutAnkiAdd = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this);
    m_shortcutAnkiAdd->setContext(Qt::WidgetWithChildrenShortcut);
    m_shortcutAnkiAdd->setEnabled(false);

    FlowLayout *frequencies = new FlowLayout(-1, 6);
    for (const Frequency &freq : m_kanji->frequencies)
    {
        frequencies->addWidget(new TagWidget(freq));
    }
    layoutParent->addLayout(frequencies);

    QVBoxLayout *layoutDefinitions = new QVBoxLayout;
    layoutParent->addLayout(layoutDefinitions);

    QFrame *line = nullptr;
    for (const KanjiDefinition &def : m_kanji->definitions)
    {
        if (line)
            layoutDefinitions->addWidget(line);
        buildDefinitionLabel(def, layoutDefinitions);
        line = createLine();
    }
    delete line;

    layoutParent->addStretch();

    connect(
        m_shortcutAnkiAdd, &QShortcut::activated,
        this, qOverload<>(&KanjiWidget::addAnki)
    );

    if (!m_context->getAnkiClient()->isEnabled())
    {
        return;
    }

    QCoro::Task<AnkiReply<QList<bool>>> reply = m_context
        ->getAnkiClient()
        ->notesAddable(QList<std::shared_ptr<const Kanji>>({m_kanji}));
    QCoro::connect(
        std::move(reply),
        this,
        [this, factory] (AnkiReply<QList<bool>> &&result) -> void
        {
            if (!result.error.isEmpty())
            {
                return;
            }

            if (result.value.first()) /* Kanji Addable */
            {
                m_buttonAnkiAddOpen->setIcon(
                    factory->getIcon(IconFactory::Icon::plus)
                );
                m_buttonAnkiAddOpen->setToolTip("Add Anki note");
                connect(
                    m_buttonAnkiAddOpen, &QToolButton::clicked,
                    this, qOverload<>(&KanjiWidget::addAnki)
                );
                m_buttonAnkiAddOpen->show();
                m_shortcutAnkiAdd->setEnabled(true);
            }
            else /* Kanji Already Added */
            {
                m_buttonAnkiAddOpen->setIcon(
                    factory->getIcon(IconFactory::Icon::hamburger)
                );
                m_buttonAnkiAddOpen->setToolTip("Show in Anki");
                connect(
                    m_buttonAnkiAddOpen, &QToolButton::clicked,
                    this, qOverload<>(&KanjiWidget::openAnki)
                );
                m_buttonAnkiAddOpen->show();
                m_shortcutAnkiAdd->setEnabled(false);
            }
        }
    );
}

/* Begin Constructor/Destructor */
/* Begin Builders */

void KanjiWidget::buildDefinitionLabel(const KanjiDefinition &def,
                                             QVBoxLayout     *layout)
{
    /* Add Tags */
    FlowLayout *tagLayout = new FlowLayout(-1, 6);
    layout->addLayout(tagLayout);
    for (const Tag &tag : def.tags)
    {
        tagLayout->addWidget(new TagWidget(tag));
    }
    tagLayout->addWidget(new TagWidget(def.dictionary));

    /* Add Glossary, Reading, Statistics Header */
    QGridLayout *layoutGLS = new QGridLayout;
    layout->addLayout(layoutGLS);
    layoutGLS->addWidget(createLine(), 0, 0, 1, -1);
    layoutGLS->addWidget(createLabel("Glossary",   true), 1, 0);
    layoutGLS->addWidget(createLabel("Reading",    true), 1, 1);
    layoutGLS->addWidget(createLabel("Statistics", true), 1, 2);
    layoutGLS->addWidget(createLine(), 2, 0, 1, -1);

    /* Add Glossary Entries */
    QString text;
    for (int i = 0; i < def.glossary.size(); ++i)
    {
        text += QString::number(i + 1);
        text += ". ";
        text += def.glossary[i];
        text += "<br>";
    }
    layoutGLS->addWidget(createLabel(text));

    /* Add Readings */
    text.clear();
    /* Add Onyomi (Chinese) */
    for (const QString &str : def.onyomi)
    {
        text += str + '\n';
    }
    text += '\n';
    /* Add Kunyomi (Japanese) */
    for (const QString &str : def.kunyomi)
    {
        text += str + '\n';
    }
    text.chop(1);
    layoutGLS->addWidget(createLabel(text));

    /* Add Statistics */
    QVBoxLayout *layoutStats = new QVBoxLayout;
    layoutGLS->addLayout(
        layoutStats, layoutGLS->rowCount() - 1, layoutGLS->columnCount() - 1
    );
    for (const QPair<Tag, QString> &kv : def.stats)
    {
        layoutStats->addLayout(createKVLabel(kv.first.notes, kv.second));
    }
    layoutStats->addStretch();

    /* Add Everything Else */
    addKVSection("Classifications", def.clas, layout);
    addKVSection("Codepoints", def.code, layout);
    addKVSection("Dictionary Indices", def.index, layout);
}

void KanjiWidget::addKVSection(const QString &title,
                               const QList<QPair<Tag, QString>> &pairs,
                               QVBoxLayout *layout)
{
    if (pairs.isEmpty())
        return;

    layout->addWidget(createLine());
    layout->addWidget(createLabel(title, true));
    layout->addWidget(createLine());
    for (const QPair<Tag, QString> &kv : pairs)
    {
        layout->addLayout(createKVLabel(kv.first.notes, kv.second));
    }
}

/* End Builders */
/* Begin Button Handlers */

void KanjiWidget::addAnki()
{
    m_buttonAnkiAddOpen->setEnabled(false);
    m_shortcutAnkiAdd->setEnabled(false);

    QCoro::Task<bool> addTask = addAnki(m_context, initAnkiKanji());
    QCoro::connect(
        std::move(addTask),
        this,
        [this] (bool success) -> void
        {
            if (!success)
            {
                return;
            }
            m_buttonAnkiAddOpen->disconnect();
            m_buttonAnkiAddOpen->setIcon(
                IconFactory::create()->getIcon(IconFactory::Icon::hamburger)
            );
            m_buttonAnkiAddOpen->setToolTip("Show in Anki");
            connect(
                m_buttonAnkiAddOpen, &QToolButton::clicked,
                this, qOverload<>(&KanjiWidget::openAnki)
            );
            m_buttonAnkiAddOpen->setEnabled(true);
            m_shortcutAnkiAdd->setEnabled(false);
        }
    );
}

void KanjiWidget::openAnki()
{
    openAnki(m_context, initAnkiKanji());
}

/* End Button Handler */
/* Begin Helpers */

QFrame *KanjiWidget::createLine() const
{
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setLineWidth(1);
    return line;
}

QLabel *KanjiWidget::createLabel(const QString &text,
                                 const bool bold,
                                 const Qt::AlignmentFlag alignment) const
{
    QLabel *label = new QLabel;
    label->setText(text);
    label->setAlignment(alignment);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);

    if (bold)
    {
        label->setStyleSheet(
            "QLabel {"\
                "font-weight: bold;"\
            "}"
        );
    }

    return label;
}

QLayout *KanjiWidget::createKVLabel(const QString &key,
                                    const QString &value) const
{
    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(createLabel(key, true));
    QLabel *vLabel = createLabel(value, false, Qt::AlignRight);
    vLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    layout->addWidget(vLabel);
    return layout;
}

std::unique_ptr<Kanji> KanjiWidget::initAnkiKanji() const
{
    PlayerAdapter *player = m_context->getPlayerAdapter();
    SubtitleListWidget *subList = m_context->getSubtitleListWidget();

    std::unique_ptr<Kanji> kanji = std::make_unique<Kanji>(*m_kanji);
    double delay = player->getSubDelay() - player->getAudioDelay();
    kanji->clipboard = QGuiApplication::clipboard()->text();
    kanji->title = player->getTitle();
    kanji->sentence = player->getSubtitle(true);
    kanji->sentence2 = player->getSecondarySubtitle();
    kanji->startTime = player->getSubStart() + delay;
    kanji->endTime = player->getSubEnd() + delay;
    kanji->context = subList->getPrimaryContext("\n");
    kanji->context2 = subList->getSecondaryContext("\n");
    QPair<double, double> contextTimes = subList->getPrimaryContextTime();
    kanji->startTimeContext = contextTimes.first + delay;
    kanji->endTimeContext = contextTimes.second + delay;
    return kanji;
}

/* End Helpers */
/* Begin Static Methods */

QCoro::Task<bool> KanjiWidget::addAnki(
    Context *context,
    std::unique_ptr<Kanji> kanji)
{
    AnkiReply<int> result =
        co_await context->getAnkiClient()->addNote(std::move(kanji));
    if (!result.error.isEmpty())
    {
        emit context->showCritical("Error Adding Note", result.error);
        co_return false;
    }
    co_return true;
}

QCoro::Task<bool> KanjiWidget::openAnki(
    Context *context,
    std::unique_ptr<Kanji> kanji)
{
    AnkiReply<QList<int>> result =
        co_await context->getAnkiClient()->openDuplicates(std::move(kanji));
    if (!result.error.isEmpty())
    {
        emit context->showCritical("Error Opening Anki", result.error);
        co_return false;
    }
    co_return true;
}

/* End Static Methods */
