**[AS] WarnSystem** - позволяет админам выдавать предупреждения нарушителям и автоматически банит при достижении лимита.

Команды:
- `!warn` - открывает меню для выдачи и снятия предупреждений.
- `!warns` - позволяет просмотреть список своих активных предупреждений.

В файл `admin_system.phrases.txt` обязательно необходимо добавить следующее:
```ini
 	"Chat_Prefix"
	{
		"ru"	"{RED}[WarnSystem] {DEFAULT}"
		"ua"	"{RED}[WarnSystem] {DEFAULT}"
		"en"	"{RED}[WarnSystem] {DEFAULT}"
	} 	  
	"Chat_WarnReceived"
	{
		"ru"	"Вы получили предупреждение (%i/%i): %s. Подробнее: !warns"
		"ua"	"Ви отримали попередження (%i/%i): %s. Детальніше: !warns"
		"en"	"You received a warning (%i/%i): %s. Details: !warns"
	}
	"Chat_WarnIssued"
	{
		"ru"	"Вы выдали предупреждение игроку %s (%i/%i)"
		"ua"	"Ви видали попередження гравцю %s (%i/%i)"
		"en"	"You issued a warning to %s (%i/%i)"
	}
	"Chat_WarnBan"
	{
		"ru"	"%s забанен за превышение лимита предупреждений"
		"ua"	"%s забанений за перевищення ліміту попереджень"
		"en"	"%s was banned for reaching the warning limit"
	}
	"Chat_NoWarnings"
	{
		"ru"	"Для этого игрока нет предупреждений"
		"ua"	"Для цього гравця немає попереджень"
		"en"	"No warnings to remove for this player"
	}
	"Chat_WarnRemovedTarget"
	{
		"ru"	"Одно предупреждение снято. Текущее количество: %i. Подробнее: !warns"
		"ua"	"Одне попередження знято. Поточна кількість: %i. Детальніше: !warns"
		"en"	"One warning was removed. Current: %i. Details: !warns"
	}
	"Chat_WarnRemovedAdmin"
	{
		"ru"	"Снято одно предупреждение с игрока %s"
		"ua"	"Знято одне попередження з гравця %s"
		"en"	"Removed one warning from %s"
	}
	"Unit_Day"
	{
		"ru"	"дн"
		"ua"	"дн"
		"en"	"d"
	}
	"Unit_Hour"
	{
		"ru"	"ч"
		"ua"	"год"
		"en"	"h"
	}
	"Unit_Min"
	{
		"ru"	"мин"
		"ua"	"хв"
		"en"	"m"
	}
	"Unit_Sec"
	{
		"ru"	"сек"
		"ua"	"с"
		"en"	"s"
	}	
	"Menu_ItemWarnSystem"
	{
		"ru"	"Система предупреждений"
		"ua"	"Система попереджень"
		"en"	"Warn System"
	}	
	"Menu_SelectReason"
	{
		"ru"	"Выберите причину предупреждения"
		"ua"	"Оберіть причину попередження"
		"en"	"Select warning reason"
	}
	"Menu_WarningsFor"
	{
		"ru"	"Предупреждения для %s (%i/%i)"
		"ua"	"Попередження для %s (%i/%i)"
		"en"	"Warnings for %s (%i/%i)"
	}
	"Menu_IssueWarning"
	{
		"ru"	"Выдать предупреждение"
		"ua"	"Видати попередження"
		"en"	"Issue warning"
	}
	"Menu_RemoveWarning"
	{
		"ru"	"Снять последнее предупреждение"
		"ua"	"Зняти останнє попередження"
		"en"	"Remove last warning"
	}
	"Menu_SelectPlayer"
	{
		"ru"	"Выберите игрока"
		"ua"	"Оберіть гравця"
		"en"	"Select player"
	}
	"Menu_RemoveSelect"
	{
		"ru"	"Выберите предупреждение для снятия"
		"ua"	"Виберіть попередження для зняття"
		"en"	"Select warning to remove"
	}	
	"Menu_NoPlayers"
	{
		"ru"	"Нет доступных игроков"
		"ua"	"Немає доступних гравців"
		"en"	"No players available"
	}
	"Menu_MyWarnings"
	{
		"ru"	"Ваши предупреждения (%i)"
		"ua"	"Ваші попередження (%i)"
		"en"	"Your warnings (%i)"
	}
	"Menu_WarnExpires"
	{
		"ru"	"Истекает: %s"
		"ua"	"Спливає: %s"
		"en"	"Expires: %s"
	}
	"Menu_WarnExpireNever"
	{
		"ru"	"Не истекает"
		"ua"	"Не спливає"
		"en"	"Does not expire"
	}	
	"Menu_ConfirmReason"
	{
		"ru"	"Подтвердите предупреждение"
		"ua"	"Підтвердіть попередження"
		"en"	"Confirm warning"
	}
	"Menu_WarnDetail"
	{
		"ru"	"Предупреждение #%i"
		"ua"	"Попередження #%i"
		"en"	"Warning #%i"
	}
	"Menu_NoWarningsSelf"
	{
		"ru"	"У вас нет предупреждений"
		"ua"	"У вас немає попереджень"
		"en"	"You have no warnings"
	}
```

Требования:
- [Utils](https://github.com/Pisex/cs2-menus/releases)
- [Admin System](https://github.com/Pisex/cs2-admin_system/releases)
