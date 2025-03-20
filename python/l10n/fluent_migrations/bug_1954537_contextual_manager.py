# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from
from fluent.migrate.transforms import TransformPattern
import fluent.syntax.ast as FTL


class REPLACE_TOTAL(TransformPattern):
    def visit_TextElement(self, node):
        node.value = node.value.replace("$count", "$total")
        return node

    def visit_SelectExpression(self, node):
        if node.selector.id.name == "count":
            node.selector.id.name = "total"
        return node


def migrate(ctx):
    """Bug 1954537 - Expose contextual-manager.ftl to localization, part {index}."""

    source_logins = "browser/browser/aboutLogins.ftl"
    target = "browser/browser/contextual-manager.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("contextual-manager-passwords-remove-all-confirm"),
                value=REPLACE_TOTAL(
                    source_logins,
                    "about-logins-confirm-remove-all-dialog-checkbox-label2",
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "contextual-manager-passwords-remove-all-confirm-button"
                ),
                value=REPLACE_TOTAL(
                    source_logins,
                    "about-logins-confirm-remove-all-dialog-confirm-button-label",
                ),
            ),
        ],
    )

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
contextual-manager-filter-input =
  .placeholder = {COPY_PATTERN(from_path, "about-logins-login-filter2.placeholder")}
  .key = {COPY_PATTERN(from_path, "about-logins-login-filter2.key")}
  .aria-label = {COPY_PATTERN(from_path, "about-logins-login-filter2.placeholder")}
contextual-manager-passwords-command-create = {COPY_PATTERN(from_path, "create-login-button.title")}
contextual-manager-passwords-command-import-from-browser = {COPY_PATTERN(from_path, "about-logins-menu-menuitem-import-from-another-browser")}
contextual-manager-passwords-command-import = {COPY_PATTERN(from_path, "about-logins-menu-menuitem-import-from-a-file")}
contextual-manager-passwords-command-help = {COPY_PATTERN(from_path, "about-logins-menu-menuitem-help")}
contextual-manager-passwords-export-os-auth-dialog-message-win = {COPY_PATTERN(from_path, "about-logins-export-password-os-auth-dialog-message2-win")}
contextual-manager-passwords-export-os-auth-dialog-message-macosx = {COPY_PATTERN(from_path, "about-logins-export-password-os-auth-dialog-message2-macosx")}
contextual-manager-passwords-reveal-password-os-auth-dialog-message-win = {COPY_PATTERN(from_path, "about-logins-reveal-password-os-auth-dialog-message-win")}
contextual-manager-passwords-reveal-password-os-auth-dialog-message-macosx = {COPY_PATTERN(from_path, "about-logins-reveal-password-os-auth-dialog-message-macosx")}
contextual-manager-passwords-edit-password-os-auth-dialog-message-win = {COPY_PATTERN(from_path, "about-logins-edit-login-os-auth-dialog-message2-win")}
contextual-manager-passwords-edit-password-os-auth-dialog-message-macosx = {COPY_PATTERN(from_path, "about-logins-edit-login-os-auth-dialog-message2-macosx")}
contextual-manager-passwords-copy-password-os-auth-dialog-message-win = {COPY_PATTERN(from_path, "about-logins-copy-password-os-auth-dialog-message-win")}
contextual-manager-passwords-copy-password-os-auth-dialog-message-macosx = {COPY_PATTERN(from_path, "about-logins-copy-password-os-auth-dialog-message-macosx")}
contextual-manager-passwords-import-file-picker-import-button = {COPY_PATTERN(from_path, "about-logins-import-file-picker-import-button")}
contextual-manager-passwords-import-file-picker-csv-filter-title = {COPY_PATTERN(from_path, "about-logins-import-file-picker-csv-filter-title")}
contextual-manager-passwords-import-file-picker-tsv-filter-title = {COPY_PATTERN(from_path, "about-logins-import-file-picker-tsv-filter-title")}
contextual-manager-passwords-import-success-button = {COPY_PATTERN(from_path, "about-logins-import-dialog-done")}
contextual-manager-passwords-import-error-button-cancel = {COPY_PATTERN(from_path, "about-logins-import-dialog-error-cancel")}
contextual-manager-passwords-export-success-button = {COPY_PATTERN(from_path, "about-logins-import-dialog-done")}
contextual-manager-export-passwords-dialog-confirm-button = {COPY_PATTERN(from_path, "about-logins-confirm-export-dialog-confirm-button2")}
contextual-manager-passwords-export-file-picker-title = {COPY_PATTERN(from_path, "about-logins-export-file-picker-title2")}
contextual-manager-passwords-export-file-picker-export-button = {COPY_PATTERN(from_path, "about-logins-export-file-picker-export-button")}
contextual-manager-passwords-export-file-picker-csv-filter-title = {COPY_PATTERN(from_path, "about-logins-import-file-picker-csv-filter-title")}
contextual-manager-passwords-count = {COPY_PATTERN(from_path, "login-list-count2")}
contextual-manager-passwords-filtered-count = {COPY_PATTERN(from_path, "login-list-filtered-count2")}
contextual-manager-passwords-update-password-success-button = {COPY_PATTERN(from_path, "about-logins-import-dialog-done")}
contextual-manager-passwords-delete-password-success-button = {COPY_PATTERN(from_path, "about-logins-import-dialog-done")}
contextual-manager-passwords-remove-login-card-title = {COPY_PATTERN(from_path, "about-logins-confirm-delete-dialog-title")}
contextual-manager-passwords-remove-login-card-remove-button = {COPY_PATTERN(from_path, "about-logins-login-item-remove-button")}
contextual-manager-passwords-remove-login-card-cancel-button = {COPY_PATTERN(from_path, "login-item-cancel-button")}
contextual-manager-passwords-create-label =
  .label = {COPY_PATTERN(from_path, "create-login-button.title")}
contextual-manager-passwords-list-label =
  .aria-label = {COPY_PATTERN(from_path, "about-logins-page-title-name")}
contextual-manager-copy-icon =
  .alt = {COPY_PATTERN(from_path, "login-item-copy-password-button-text")}
""",
            from_path=source_logins,
        ),
    )
