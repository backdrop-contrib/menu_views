<?php

/**
 * @file
 * Form hooks for the menu_views module.
 */
 
/**
 * Helper function to determine where form values for menu views are located.
 */
function _menu_views_form_parents($item) {
  return ($item['tree'] ? 'options][' : '') . 'menu_views][';
}

/**
 * Implements hook_form_FORM_ID_alter().
 * Appends the attached view to the title of the menu item.
 */
function menu_views_form_menu_overview_form_alter(&$form, &$form_state) {
  $attached = FALSE;
  $elements = element_children($form);
  foreach ($elements as $mlid) {
    $element = &$form[$mlid];
    // Only process menu items
    if (isset($element['#item'])) {
      $menu_item = &$element['#item'];
      // Only change the title for attached views
      $item = _menu_views_item($menu_item);
      if ($item['type'] == 'view' && $item['view']['name']) {
        if ($view = views_get_view($item['view']['name'])) {
          if (!$attached) {
            $form['#attached']['css'][] = drupal_get_path('module', 'menu_views') . '/menu_views.admin.css';
            $attached = TRUE;
          }
          $name = $view->human_name ? $view->human_name : $view->name;
          if ($item['view']['display'] && isset($view->display[$item['view']['display']])) {
            $display = $view->display[$item['view']['display']]->display_title;
            $arguments = _menu_views_tokenize_arguments($item, TRUE);
            if (!empty($arguments)) {
              $arguments = ' : ' . $arguments;
            }
            $disabled = $menu_item['hidden'] ? ' (' . t('disabled') . ')' : '';
            $title = '';
            if ($menu_item['link_path'] != '<view>') {
              // Must manually set the HTML here. Don't need the view showing up here by runnging it through l();
              $title = '<a href="' . check_plain(url($menu_item['href'], $menu_item['localized_options'])) . '"' . drupal_attributes($menu_item['localized_options']['attributes']) . '>' . check_plain($menu_item['title']) . '</a> ';
            }
            $element['title']['#markup'] = $title . 'View: <div class="messages status view" title="' . $item['view']['name'] . ' : ' . $item['view']['display'] . ($item['view']['arguments'] ? ' : ' . $item['view']['arguments'] : '') . '">' . $name . ' : ' . $display . $arguments . $disabled . '</div>';
          }
        }
      }
    }
  }
}

/**
 * Implements hook_form_FORM_ID_alter().
 * Change path description. Insert the views selection.
 */
function menu_views_form_menu_edit_item_alter(&$form, &$form_state) {
  // Only allow users who have the ability to use menu views see options.
  if (!user_access('administer menu views')) {
    return;
  }
  $form['#attached']['css'][] = drupal_get_path('module', 'menu_views') . '/menu_views.admin.css';
  $form['#prefix'] = '<div id="menu-edit-item-wrapper">';
  $form['#suffix'] = '</div>';
  
  // Unset the previous value so that the new values get saved.
  if (isset($form['options']['#value']['menu_views'])) {
    unset($form['options']['#value']['menu_views']);
  }
  
  // Get the settings for the currently attached view, AJAX submit values or default values.
  $item = _menu_views_item($form, $form_state);
  
  $original_path = '';
  if ($item['type'] == 'link' && $form['link_path']['#default_value'] == '<view>') {
    $form['link_path']['#default_value'] = $item['original_path'];
  }
  elseif ($item['type'] == 'view') {
    if ($form['link_path']['#default_value'] != '<view>') {
      $original_path = $form['link_path']['#default_value'];
    }
    $form['link_path']['#default_value'] = '<view>';
  }
  
  // Hide core fields.
  $form['link_title']['#access'] = FALSE;
  $form['link_path']['#access'] = FALSE;
  
  $form['menu_item_type'] = array(
    '#type' => 'radios',
    '#title' => t('Menu Item Type'),
    '#options' => array('link' => t('Link'), 'view' => t('View')),
    '#default_value' => $item['type'],
    // Add container-inline style for admin themes like Rubik.
    '#prefix' => '<div id="menu-item-type" class="form-item container-inline messages warning">',
    '#suffix' => '</div>',
    '#ajax' => array(
      'callback' => 'menu_views_ajax_callback',
      'wrapper' => 'menu-edit-item-wrapper',
    ),
    '#weight' => -1000,
  );

  // Setup the form
  $form['options']['menu_views'] = array(
    '#type' => 'container',
    '#tree' => TRUE,
    '#weight' => -10,
  );
  $form['options']['menu_views']['original_path'] = array(
    '#type' => 'value',
    '#value' => $original_path,
  );
  $form['options']['menu_views']['view'] = array(
    '#type' => 'fieldset',
    '#access' => FALSE,
  );
  $access = !($item['type'] == 'view');
  // Hide core fields for menu item.
  foreach (element_children($form) as $child) {
    // Skip options (handled below) and actions.
    if ($child == 'menu_item_type' || $child == 'enabled' || $form[$child]['#type'] == 'actions' || $form[$child]['#type'] == 'hidden' || $form[$child]['#type'] == 'value') {
      continue;
    }
    $form[$child]['#access'] = $access;
  }
  foreach (element_children($form['options']) as $child) {
    // Skip menu_view options.
    if ($child == 'menu_views' || $form['options'][$child]['#type'] == 'hidden' || $form['options'][$child]['#type'] == 'value') {
      continue;
    }
    $form['options'][$child]['#access'] = $access;
  }
  if ($item['type'] == 'view') {
    $form['options']['menu_views']['view']['#access'] = TRUE;
  }
  $options = array();
  $views = views_get_enabled_views();
  foreach ($views as $name => $view) {
    $options[$name] = $view->human_name . ' (' . $name . ')';
  }
  ksort($options);
  $form['options']['menu_views']['view']['name'] = array(
    '#type' => 'select',
    '#title' => t('View'),
    '#empty_option' => t('- None -'),
    '#description' => t('Select a view to attach.'),
    '#default_value' => $item['view']['name'],
    '#options' => $options,
    '#ajax' => array(
      'callback' => 'menu_views_ajax_callback',
      'wrapper' => 'menu-edit-item-wrapper',
    ),
  );
  if ($item['view']['name']) {
    $view = views_get_view($item['view']['name']);
    $options = array();
    foreach ($view->display as $name => $display) {
      $options[$name] = $display->display_title . ' (' . $name . ')';
    }
    $form['options']['menu_views']['view']['display'] = array(
      '#type' => 'select',
      '#title' => t('Display'),
      '#description' => t('Select a view display to use.'),
      '#empty_option' => t('- Select a display -'),
      '#default_value' => $item['view']['display'],
      '#options' => $options,
      '#ajax' => array(
        'callback' => 'menu_views_ajax_callback',
        'wrapper' => 'menu-edit-item-wrapper',
      ),
    );
    if ($item['view']['display']) {
      $form['options']['menu_views']['view']['arguments'] = array(
        '#type' => 'textfield',
        '#title' => t('Arguments'),
        '#description' => t('You can reference the node associated with this menu link by using <code>[menu-link:node:nid]</code>.<br />You can reference the node associated with the parent of this menu link by using <code>[menu-link:parent:node:nid]</code>.'),
        '#default_value' => $item['view']['arguments'],
      );
      if (module_exists('token')) {
        $form['options']['menu_views']['view']['tokens'] = array(
          '#type' => 'fieldset',
          '#title' => t('Replacement Tokens'),
          '#description' => t('Below are the list of available tokens to use for passing arguments to this attached view.<br /><br />You can reference the node associated with this menu link by using <code>[menu-link:node:nid]</code>.<br />You can reference the node associated with the parent of this menu link by using <code>[menu-link:parent:node:nid]</code>.'),
          '#collapsible' => TRUE,
          '#collapsed' => TRUE,
        );
        $form['options']['menu_views']['view']['tokens']['tokens'] = array(
          '#theme' => 'token_tree',
          '#token_types' => array('menu-link'),
        );
      }
      // Advanced Render Options.
      $form['options']['menu_views']['view']['settings'] = array(
        '#type' => 'fieldset',
        '#title' => t('Advanced Settings'),
        '#collapsible' => TRUE,
        '#collapsed' => !$item['view']['settings']['title'],
      );
      $form['options']['menu_views']['view']['settings']['title'] = array(
        '#type' => 'checkbox',
        '#title' => t('Display Title'),
        '#default_value' => $item['view']['settings']['title'],
      );
      $form['options']['menu_views']['view']['settings']['title_options'] = array(
        '#type' => 'fieldset',
        '#title' => t('Title Options'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        '#parents' => isset($form['options']['#tree']) && $form['options']['#tree'] ? array('options', 'menu_views', 'view', 'settings') : array('menu_views', 'view', 'settings'),
        '#states' => array(
          'visible' => array(
           ':input[name="options[menu_views][view][settings][title]"]' => array('checked' => TRUE),
          ),
        ),
      );
      $form['options']['menu_views']['view']['settings']['title_options']['title_wrapper'] = array(
        '#type' => 'select',
        '#title' => t('Element Wrapper'),
        '#description' => t('Choose which element wrapper to use around the title of the view. Default: H3'),
        '#default_value' => $item['view']['settings']['title_wrapper'],
        '#options' => variable_get('views_field_rewrite_elements', array(
          '' => t('- Use default -'),
          '0' => t('- None -'),
          'div' => 'DIV',
          'span' => 'SPAN',
          'h1' => 'H1',
          'h2' => 'H2',
          'h3' => 'H3',
          'h4' => 'H4',
          'h5' => 'H5',
          'h6' => 'H6',
          'p' => 'P',
          'strong' => 'STRONG',
          'em' => 'EM',
        )),
      );
      $form['options']['menu_views']['view']['settings']['title_options']['title_classes'] = array(
        '#type' => 'textfield',
        '#title' => t('Classes'),
        '#description' => t('Provide any additional classes, separated by spaces. Title must use the element wrapper above for this take effect.'),
        '#default_value' => $item['view']['settings']['title_classes'],
      );
      $form['options']['menu_views']['view']['settings']['title_options']['title_override'] = array(
        '#type' => 'textfield',
        '#title' => t('Override Title'),
        '#description' => t('Enter custom text to override title output. Default: Title will be provided from the view.'),
        '#default_value' => $item['view']['settings']['title_override'],
      );
      
    }
  }
  
  // Inject handlers
  $form['#validate'] = array_merge($form['#validate'], array('menu_views_menu_edit_item_validate'));
  
  // Remove core submit handler and use this module's handler to ensure the mlid is saved to the view for new items.
  foreach ($form['#submit'] as $key => $handle) {
    if ($handle == 'menu_edit_item_submit') {
      unset($form['#submit'][$key]);
    }
  }
  $form['#submit'] = array_merge($form['#submit'], array('menu_views_menu_edit_item_submit'));
}

/**
 * Ajax callback for re-rendering the attach views portion of the form.
 */
function menu_views_ajax_callback(&$form, &$form_state) {
  return $form;
}

/**
 * Validate handler for menu_edit_item form.
 */
function menu_views_menu_edit_item_validate($form, &$form_state) {
  // Only run this validation when the form is fully submitted.
  if ($form_state['submitted']) {
    if ($form_state['values']['link_path'] == '<view>') {
      $item = _menu_views_item($form, $form_state);
      if (!$item['view']['name']) {
        form_set_error(_menu_views_form_parents($item) . 'name', t('You must select a view before this menu item can be saved.'));
      }
      elseif (!$item['view']['display']) {
        form_set_error(_menu_views_form_parents($item) . 'display', t('You must select a display before this menu item can be saved.'));
      }
    }
  }
}

/**
 * Submit handler for menu_edit_item form.
 */
function menu_views_menu_edit_item_submit($form, &$form_state) {
  $values = &$form_state['values'];
  
  // Retrieve view information.
  $item = _menu_views_item($form, $form_state);
  
  // Retrieve the original menu view information for this menu link.
  $original = array();
  if (isset($values['original_item']['options']['menu_views'])) {
    $original = $values['original_item']['options']['menu_views'];
  }
    
  // The value of "hidden" is the opposite of the value
  // supplied by the "enabled" checkbox.
  $values['hidden'] = (int) !$values['enabled'];
  unset($values['enabled']);
  
  // Only set the title attribute if the menu_attributes moduule is not present.
  if (!module_exists('menu_attributes')) {
    $values['options']['attributes']['title'] = $values['description'];
  }
  
  // Set the menu name and plid.
  list($values['menu_name'], $values['plid']) = explode(':', $values['parent']);
  
  // Recreate the menu view item data from the current form so only the important information is saved.
  $item = array(
    // The mlid should remain constant, always use the information provided by the menu module and not this one.
    'mlid' => $values['mlid'],
    'type' => $item['type'],
    'original' => $item['original'],
    'view' => $item['view'],
  );

  // Save the menu view information in menu link options.
  $values['options']['menu_views'] = $item;
  
  // Remove this unecessary property from the values.
  if (isset($values['menu_views'])) {
    unset($values['menu_views']);
  }
  // If this is a new menu link, save the initial menu link to return the mlid before we re-save the attached view.
  if (!$item['mlid']) {
    $item['mlid'] = menu_link_save($values);
  }
  // Save the menu link and display the appropriate status message.
  if (!menu_link_save($values)) {
    drupal_set_message(t('There was an error saving the menu link.'), 'error');
  }
  else {
    // Compare the original view information with the new one and determine if the menu's cache should be cleared.
    $diff = array_diff_assoc($item, $original);
    if (!empty($diff)) {
      // Clear the affected menu cache so the view can render properly.
      menu_cache_clear($values['menu_name']);
      _menu_clear_page_cache();
      drupal_set_message(t('Menu caches have been cleared.'));
    }
    drupal_set_message(t('Your configuration has been saved.'));
  }
    
  // Redirect to the menu list.
  $form_state['redirect'] = 'admin/structure/menu/manage/' . $values['menu_name'];
}
