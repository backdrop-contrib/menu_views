(function ($) {
  
  /**
   * Override default menuFieldsetSummaries.
   */
  Drupal.behaviors.menuFieldsetSummaries = {
    attach: function (context) {
      $('fieldset.menu-link-form', context).drupalSetSummary(function (context) {
        if ($('.form-item-menu-enabled input', context).is(':checked')) {
          var menuItemType = $('#edit-menu-menu-item-type');
          if (menuItemType.length) {
            if ($('input:checked', menuItemType).val() == 'view') {
              var summary = '';
              var name = $('#edit-menu-views-view-name, #edit-options-menu-views-view-name, #edit-menu-menu-views-view-name, #edit-menu-options-menu-views-view-name', context).first();
              if (name.length) {
                var nameValue = Drupal.checkPlain($(':selected', name).val());
                if (nameValue != '') {
                  summary = nameValue;
                  var display = $('#edit-menu-views-view-display, #edit-options-menu-views-view-display, #edit-menu-menu-views-view-display, #edit-menu-options-menu-views-view-display', context).first();
                  if (display.length) {
                    var displayValue = Drupal.checkPlain($(':selected', display).val());
                    if (displayValue != '') {
                      summary += '-' + displayValue;
                    }
                    var arguments = $('#edit-menu-views-view-arguments, #edit-options-menu-views-view-arguments, #edit-menu-menu-views-view-arguments, #edit-menu-options-menu-views-view-arguments', context).first();
                    if (arguments.length) {
                      var argumentsValue = Drupal.checkPlain(arguments.val());
                      if (argumentsValue != '') {
                        summary += '-' + argumentsValue;
                      }
                    }
                  }
                }
              }
              if (summary == '') {
                summary = Drupal.t('None');
              }
              return Drupal.t('View') + ': ' + summary;
            }
            else {
              var title = $('.form-item-menu-link-title input', context);
              if (title.length) {
                return Drupal.checkPlain(title.val());
              }
            }
          }
        }
        else {
          return Drupal.t('Not in menu');
        }
        return '';
      });
    }
  };
  
  /**
   * Move menu item settings fieldset to right column on node edit form, if using rubik.
   */
  Drupal.behaviors.menu_views = {
    attach: function (context, settings) {
      if (!settings.menu_views.node_form && settings.menu_views.admin_theme == 'rubik') {
        var sidebar = $('.column-side .column-wrapper', context);
        if (sidebar.length) {
          $('fieldset.menu-item-settings', sidebar).remove();
          $('fieldset.menu-item-settings', context).appendTo(sidebar);
        }
      }
    }
  };
}(jQuery));