(function ($) {
  /**
   * Move menu item settings fieldset to right column on node edit form, if using rubik.
   */
  Drupal.behaviors.menu_views = {
    attach: function (context, settings) {
      var sidebar = $('.column-side .column-wrapper', context);
      if (sidebar.length) {
        $('fieldset.edit-item-container', sidebar).remove();
        $('fieldset.edit-item-container', context).appendTo(sidebar);
      }
    }
  };
}(jQuery));